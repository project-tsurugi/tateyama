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

namespace tateyama::common::stream {

class connection_socket;
class stream_data_channel;

class stream_socket
{
    constexpr static int BUFFER_SIZE = 512;

public:
    enum class stream_type : unsigned char {
        undefined = 0,
        session = 1,
        resultset = 2,
    };
    static constexpr unsigned char STATUS_OK = 0;
    static constexpr unsigned char STATUS_NG = 1;

    explicit stream_socket(connection_socket* server_socket, int socket)
        : server_socket_(server_socket), socket_(socket), data_buffer_(&buffer_[0]) {
        unsigned char info{};
        std::string_view data;
        if (!recv(info, data)) {
            std::abort();
        }
        type_ = static_cast<stream_type>(info);
        name_ = std::string(data.data(), data.length());
    }

    ~stream_socket() { close(socket_); }

    stream_socket(stream_socket const& other) = delete;
    stream_socket& operator=(stream_socket const& other) = delete;
    stream_socket(stream_socket&& other) noexcept = delete;
    stream_socket& operator=(stream_socket&& other) noexcept = delete;

    bool await(unsigned char& info) {
        fd_set fds;
        FD_ZERO(&fds);  // NOLINT
        FD_SET(socket_, &fds);  // NOLINT
        select(socket_ + 1, &fds, nullptr, nullptr, nullptr);

        if (FD_ISSET(socket_, &fds)) {  // NOLINT
            auto size_i = ::recv(socket_, &info, 1, 0);
            if (size_i == 0) {
                return false;
            }
        }
        return true;
    }
    void recv(std::string_view& payload) {
        std::int64_t size_l = ::recv(socket_, &buffer_[0], sizeof(int), 0);
        while (size_l < static_cast<std::int64_t>(sizeof(int))) {
            size_l += ::recv(socket_, &buffer_[size_l], sizeof(int) - size_l, 0);  // NOLINT
        }
        unsigned int length = (buffer_[3] << 24) | (buffer_[2] << 16) | (buffer_[1] << 8) | buffer_[0];  // NOLINT

        if (length > 0) {
            if (length > BUFFER_SIZE) {
                if (data_buffer_ != &buffer_[0]) {
                    delete data_buffer_;
                }
                data_buffer_ = new char[length];  // NOLINT
            }
            auto size_v = ::recv(socket_, data_buffer_, length, 0);
            while (size_v < length) {
                size_v += ::recv(socket_, data_buffer_ + size_v, length - size_v, 0);  // NOLINT
            }
            payload = std::string_view(data_buffer_, length);
        }
    }
    bool recv(unsigned char& info, std::string_view& payload) {
        if (!await(info)) {
            return false;
        }
        recv(payload);
        return true;
    }
    void send(unsigned char info, std::string_view payload) {
        ::send(socket_, &info, 1, 0);

        unsigned int length = payload.length();
        for(std::size_t i = 0 ; i < sizeof(length); i++) {
            buffer_[i] = (length << (i * 8)) & 0xff;  // NOLINT
        }
        ::send(socket_, &buffer_[0], sizeof(length), 0);
        ::send(socket_, payload.data(), length, 0);
    }
    void send(unsigned char info) {
        ::send(socket_, &info, 1, 0);

        unsigned int length = 0;
        for(std::size_t i = 0 ; i < sizeof(length); i++) {
            buffer_[i] = (length << (i * 8)) & 0xff;  // NOLINT
        }
        ::send(socket_, &buffer_[0], sizeof(length), 0);
    }
    [[nodiscard]] stream_type get_type() const { return type_; }
    [[nodiscard]] std::string_view get_name() const { return name_; }
    [[nodiscard]] connection_socket& get_connection_socket() const { return *server_socket_; }

// for session stream
    void close_session() { session_closed_ = true; }
    [[nodiscard]] bool is_session_closed() const { return session_closed_; }

private:
    connection_socket* server_socket_;
    int socket_;
    stream_type type_;
    std::string name_;
    char buffer_[BUFFER_SIZE]{};  // NOLINT
    char *data_buffer_{};
    bool session_closed_{false};
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
        bind(socket_, (struct sockaddr *) &socket_address, sizeof(socket_address));  // NOLINT

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
            return std::make_unique<stream_socket>(this, ts);
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

    void register_resultset(const std::string& name, stream_data_channel* data_channel) {
        resultset_relations_.emplace(name, data_channel);
    }
    stream_data_channel* search_resultset(std::string_view name) {
        auto itr = resultset_relations_.find(std::string(name));
        if (itr == resultset_relations_.end()) {
            return nullptr;
        }
        auto* rv = itr->second;
        resultset_relations_.erase(itr);
        return rv;
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
    std::unordered_map<std::string, stream_data_channel*> resultset_relations_{};
};

};  // namespace tateyama::common::stream
