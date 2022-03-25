/*
 * Copyright 2019-2022 tsurugi project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <iterator>
#include <mutex>
#include <glog/logging.h>

#include <tateyama/logging.h>

namespace tateyama::common::stream {

class connection_socket;
class stream_data_channel;

class stream_socket
{
    static constexpr unsigned char REQUEST_SESSION_HELLO = 1;
    static constexpr unsigned char REQUEST_SESSION_PAYLOAD = 2;
    static constexpr unsigned char REQUEST_RESULT_SET_HELLO = 3;

    static constexpr unsigned char RESPONSE_SESSION_PAYLOAD = 1;
    static constexpr unsigned char RESPONSE_RESULT_SET_PAYLOAD = 2;
    static constexpr unsigned char RESPONSE_SESSION_HELLO_OK = 3;
    static constexpr unsigned char RESPONSE_SESSION_HELLO_NG = 4;
    static constexpr unsigned char RESPONSE_RESULT_SET_HELLO_OK = 5;
    static constexpr unsigned char RESPONSE_RESULT_SET_HELLO_NG = 6;

public:
    explicit stream_socket(int socket) : socket_(socket) {
        unsigned char info{};
        unsigned char slot{};
        std::string dummy;

        if (!recv(info, slot, dummy)) {
            std::abort();
        }
        if (info != REQUEST_SESSION_HELLO) {
            std::abort();
        }
    }
    ~stream_socket() { close(socket_); }

    stream_socket(stream_socket const& other) = delete;
    stream_socket& operator=(stream_socket const& other) = delete;
    stream_socket(stream_socket&& other) noexcept = delete;
    stream_socket& operator=(stream_socket&& other) noexcept = delete;

    bool await(unsigned char& slot) {
        unsigned char info;
        return await(info, slot);
    }
    void recv(std::string& payload) {
        char buffer[4];  // NOLINT

        std::int64_t size_l = ::recv(socket_, &buffer[0], sizeof(int), 0);
        while (size_l < static_cast<std::int64_t>(sizeof(int))) {
            size_l += ::recv(socket_, &buffer[size_l], sizeof(int) - size_l, 0);  // NOLINT
        }
        unsigned int length = (strip(buffer[3]) << 24) | (strip(buffer[2]) << 16) | (strip(buffer[1]) << 8) | strip(buffer[0]);  // NOLINT

        if (length > 0) {
            payload = std::string(length, '\0');
            char *data_buffer = payload.data();
            auto size_v = ::recv(socket_, data_buffer, length, 0);
            while (size_v < length) {
                size_v += ::recv(socket_, data_buffer + size_v, length - size_v, 0);  // NOLINT
            }
        }
    }

    void register_resultset(const std::string& name, stream_data_channel* data_channel) {  // for REQUEST_RESULTSET_HELLO
        resultset_relations_.emplace(name, data_channel);
    }
    
    void send(std::string_view payload) {  // for RESPONSE_SESSION_HELLO_OK
        std::unique_lock<std::mutex> lock(mutex_);
        send_session_response(RESPONSE_SESSION_HELLO_OK, 0, payload);
    }
    void send(unsigned char slot, std::string_view payload) {  // for RESPONSE_SESSION_PAYLOAD
        std::unique_lock<std::mutex> lock(mutex_);
        send_session_response(RESPONSE_SESSION_PAYLOAD, slot, payload);
    }
    void send(unsigned char slot, unsigned char writer, std::string_view payload) { // for RESPONSE_RESULT_SET_PAYLOAD
        std::unique_lock<std::mutex> lock(mutex_);
        unsigned char info = RESPONSE_RESULT_SET_PAYLOAD;
        ::send(socket_, &info, 1, 0);
        ::send(socket_, &slot, 1, 0);
        ::send(socket_, &writer, 1, 0);
        send_payload(payload);
    }

// for session stream
    void close_session() { session_closed_ = true; }
    [[nodiscard]] bool is_session_closed() const { return session_closed_; }

private:
    int socket_;
    bool session_closed_{false};
    std::unordered_map<std::string, stream_data_channel*> resultset_relations_{};
    std::mutex mutex_{};

    bool await(unsigned char& info, unsigned char& slot) {
        while (true) {
            fd_set fds;
            FD_ZERO(&fds);  // NOLINT
            FD_SET(socket_, &fds);  // NOLINT
            select(socket_ + 1, &fds, nullptr, nullptr, nullptr);

            if (FD_ISSET(socket_, &fds)) {  // NOLINT
                if (auto size_i = ::recv(socket_, &info, 1, 0); size_i == 0) {
                    return false;
                }
                if (auto size_i = ::recv(socket_, &slot, 1, 0); size_i == 0) {
                    std::abort();
                }
            }
            if (info == REQUEST_RESULT_SET_HELLO) {
                std::string name;
                recv(name);
                if (auto data_channel = search_resultset(name); data_channel != nullptr) {
                    send_result_set_hello_response(RESPONSE_RESULT_SET_HELLO_OK, slot);
                    setup_resultset(data_channel, slot);
                } else {
                    VLOG(log_error) << __func__ << ": notify client of error, result set name = " << name << std::endl;
                    send_result_set_hello_response(RESPONSE_RESULT_SET_HELLO_NG, slot);
                }
            } else {
                return true;
            }
        }
    }
    bool recv(unsigned char& info, unsigned char& slot, std::string& payload) {  // REQUEST_SESSION_HELLO and REQUEST_SESSION_PAYLOAD
        if (!await(info, slot)) {
            return false;
        }
        recv(payload);
        return true;
    }

    stream_data_channel* search_resultset(std::string_view name) {  // for REQUEST_RESULTSET_HELLO
        auto itr = resultset_relations_.find(std::string(name));
        if (itr == resultset_relations_.end()) {
            VLOG(log_error) << __func__ << ": can not find " << name << std::endl;
            return nullptr;
        }
        auto* data_channel = itr->second;
        resultset_relations_.erase(itr);
        return data_channel;
    };
    void setup_resultset(stream_data_channel*, unsigned char);  // for REQUEST_RESULTSET_HELLO

    void send_result_set_hello_response(unsigned char info, unsigned char slot) {  // for RESPONSE_RESULT_SET_HELLO_[OK|NG]
        std::unique_lock<std::mutex> lock(mutex_);

        ::send(socket_, &info, 1, 0);
        ::send(socket_, &slot, 1, 0);

        unsigned int length = 0;
        char buffer[4];  // NOLINT
        for(std::size_t i = 0 ; i < sizeof(length); i++) {
            buffer[i] = (length << (i * 8)) & 0xff;  // NOLINT
        }
        ::send(socket_, &buffer[0], sizeof(length), 0);
    }
    void send_session_response(unsigned char info, unsigned char slot, std::string_view payload) {
        ::send(socket_, &info, 1, 0);
        ::send(socket_, &slot, 1, 0);
        send_payload(payload);
    }
    void send_payload(std::string_view payload) {
        unsigned int length = payload.length();
        char buffer[4];  // NOLINT
        buffer[0] = length & 0xff;  // NOLINT
        buffer[1] = (length / 0x100) & 0xff;  // NOLINT
        buffer[2] = (length / 0x10000) & 0xff;  // NOLINT
        buffer[3] = (length / 0x1000000) & 0xff;  // NOLINT
        ::send(socket_, &buffer[0], sizeof(length), 0);
        ::send(socket_, payload.data(), length, 0);
    }

    std::size_t strip(char c) {
        return (static_cast<std::uint32_t>(c) & 0xff);  // NOLINT
    }
};

// implements connect operation
class connection_socket
{
public:
    /**
     * @brief Construct a new object.
     */
    connection_socket() = delete;
    explicit connection_socket(std::uint32_t port) {
        // create a pipe
        if (pipe(&pair_[0]) != 0) {
            std::abort();
        }

        // create a socket
        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);

        // Map the address and the port to the socket
        struct sockaddr_in socket_address{};
        socket_address.sin_family = AF_INET;
        socket_address.sin_port = htons(port);
        socket_address.sin_addr.s_addr = INADDR_ANY;
        if (bind(socket_, (struct sockaddr *) &socket_address, sizeof(socket_address)) != 0) {  // NOLINT
            VLOG(log_error) << "bind error, probably another server is running";
            std::abort();
        }
        // listen the port
        listen(socket_, SOMAXCONN);
    }
    ~connection_socket() = default;

    /**
     * @brief Copy and move constructers are deleted.
     */
    connection_socket(connection_socket const&) = delete;
    connection_socket(connection_socket&&) = delete;
    connection_socket& operator = (connection_socket const&) = delete;
    connection_socket& operator = (connection_socket&&) = delete;

    std::unique_ptr<stream_socket> accept([[maybe_unused]] bool wait = false) {
        fd_set fds;
        FD_ZERO(&fds);  // NOLINT
        FD_SET(socket_, &fds);  // NOLINT
        FD_SET(pair_[0], &fds);  // NOLINT
        select(((pair_[0] > socket_) ? pair_[0] : socket_) + 1, &fds, nullptr, nullptr, nullptr);

        if (FD_ISSET(socket_, &fds)) {  // NOLINT
            // Accept a connection request
            struct sockaddr_in address{};
            unsigned int len = sizeof(address);
            int ts = ::accept(socket_, (struct sockaddr *)&address, &len);  // NOLINT
            return std::make_unique<stream_socket>(ts);
        }
        if (FD_ISSET(pair_[0], &fds)) {  //  NOLINT
            char trash{};
            if (read(pair_[0], &trash, sizeof(trash)) <= 0) {
                std::abort();
            }
            return nullptr;
        }
        std::abort();
    }

    void request_terminate() {
        if (write(pair_[1], "q", 1) <= 0) {
            std::abort();
        }
    }

    void close() const {
        ::close(socket_);
    }

private:
    int socket_;
    int pair_[2]{};  // NOLINT
};

};  // namespace tateyama::common::stream
