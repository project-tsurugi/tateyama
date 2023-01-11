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
#include <stdexcept>
#include <vector>
#include <iterator>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>

#include <glog/logging.h>

#include <tateyama/logging.h>
#include "../common/logging_helper.h"

namespace tateyama::common::stream {

class connection_socket;
class stream_data_channel;

class stream_socket
{
    static constexpr unsigned char REQUEST_SESSION_HELLO = 1;
    static constexpr unsigned char REQUEST_SESSION_PAYLOAD = 2;
    static constexpr unsigned char REQUEST_RESULT_SET_BYE_OK = 3;
    static constexpr unsigned char REQUEST_SESSION_BYE = 4;

    static constexpr unsigned char RESPONSE_SESSION_PAYLOAD = 1;
    static constexpr unsigned char RESPONSE_RESULT_SET_PAYLOAD = 2;
    static constexpr unsigned char RESPONSE_SESSION_HELLO_OK = 3;
    static constexpr unsigned char RESPONSE_SESSION_HELLO_NG = 4;
    static constexpr unsigned char RESPONSE_RESULT_SET_HELLO = 5;
    static constexpr unsigned char RESPONSE_RESULT_SET_BYE = 6;
    static constexpr unsigned char RESPONSE_SESSION_BODYHEAD = 7;
    static constexpr unsigned char RESPONSE_SESSION_BYE_OK = 8;

    static constexpr unsigned int SLOT_SIZE = 16;

    class recv_entry {
    public:
        recv_entry(unsigned char info, std::uint16_t slot, std::string payload) : info_(info), slot_(slot), payload_(std::move(payload)) {
        }
        [[nodiscard]] unsigned char info() const {
            return info_;
        }
        [[nodiscard]] std::uint16_t slot() const {
            return slot_;
        }
        [[nodiscard]] std::string payload() const {
            return payload_;
        }
    private:
        unsigned char info_;
        std::uint16_t slot_;
        std::string payload_;
    };

public:
    explicit stream_socket(int socket) noexcept : socket_(socket) {
    }

    ~stream_socket() {
        close();
    }

    stream_socket(stream_socket const& other) = delete;
    stream_socket& operator=(stream_socket const& other) = delete;
    stream_socket(stream_socket&& other) noexcept = delete;
    stream_socket& operator=(stream_socket&& other) noexcept = delete;

    [[nodiscard]] bool wait_hello(std::string_view session_name) {
        unsigned char info{};
        std::uint16_t slot{};
        std::string dummy;

        if (!await(info, slot, dummy)) {
            close();
            return false;
        }
        if (info != REQUEST_SESSION_HELLO) {
            close();
            return false;
        }
        if (decline_) {
            DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_HELLO_NG";  //NOLINT
            send_response(RESPONSE_SESSION_HELLO_NG, 0, "");
            close();
            return false;
        }
        slot_size_ = slot;
        in_use_.resize(slot_size_);
        for (unsigned int i = 0; i < slot_size_; i++) {
            in_use_.at(i) = false;
        }
        send(session_name);
        return true;
    }

    [[nodiscard]] bool await(std::uint16_t& slot, std::string& payload) {
        unsigned char info{};
        return await(info, slot, payload);
    }

    void send(std::string_view payload) {  // for RESPONSE_SESSION_HELLO_OK
        DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_HELLO_OK ";  //NOLINT
        send_response(RESPONSE_SESSION_HELLO_OK, 0, payload);
    }
    void send(std::uint16_t slot, std::string_view payload, bool body) {  // for RESPONSE_SESSION_PAYLOAD
        if (body) {
            DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_PAYLOAD " << static_cast<std::uint32_t>(slot);  //NOLINT
            send_response(RESPONSE_SESSION_PAYLOAD, slot, payload);
        } else {
            DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_BODYHEAD " << static_cast<std::uint32_t>(slot);  //NOLINT
            send_response(RESPONSE_SESSION_BODYHEAD, slot, payload);
        }
    }
    void send_result_set_hello(std::uint16_t slot, std::string_view name) {  // for RESPONSE_RESULT_SET_HELLO
        DVLOG_LP(log_trace)  << "<-- RESPONSE_RESULT_SET_HELLO " << static_cast<std::uint32_t>(slot) << ", " << name;  //NOLINT
        send_response(RESPONSE_RESULT_SET_HELLO, slot, name);
    }
    void send_result_set_bye(std::uint16_t slot) {  // for RESPONSE_RESULT_SET_BYE
        DVLOG_LP(log_trace) << "<-- RESPONSE_RESULT_SET_BYE " << static_cast<std::uint32_t>(slot);  //NOLINT
        send_response(RESPONSE_RESULT_SET_BYE, slot, "");
    }
    void send(std::uint16_t slot, unsigned char writer, std::string_view payload) { // for RESPONSE_RESULT_SET_PAYLOAD
        DVLOG_LP(log_trace) << (payload.length() > 0 ? "<-- RESPONSE_RESULT_SET_PAYLOAD " : "<-- RESPONSE_RESULT_SET_COMMIT ") << static_cast<std::uint32_t>(slot) << ", " << static_cast<std::uint32_t>(writer);  //NOLINT
        std::unique_lock<std::mutex> lock(mutex_);
        if (session_closed_) {
            return;
        }
        unsigned char info = RESPONSE_RESULT_SET_PAYLOAD;
        ::send(socket_, &info, 1, 0);
        char buffer[sizeof(std::uint16_t)];  // NOLINT
        buffer[0] = slot & 0xff;  // NOLINT
        buffer[1] = (slot / 0x100) & 0xff;  // NOLINT
        ::send(socket_, &buffer[0], sizeof(std::uint16_t), 0);
        ::send(socket_, &writer, 1, 0);
        send_payload(payload);
    }

    void close() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!socket_closed_) {
            ::close(socket_);
            socket_closed_ = true;
        }
    }

    unsigned int look_for_slot() {
        for (unsigned int i = 0; i < slot_size_; i++) {  // FIXME more efficient
            if (!in_use_.at(i)) {
                in_use_.at(i) = true;
                slot_using_++;
                return i;
            }
        }
        throw std::runtime_error("running out the slots for result set");  //NOLINT
    }

    void decline() {
        decline_ = true;
    }

private:
    int socket_;
    bool session_closed_{false};
    bool socket_closed_{false};
    bool decline_{false};
    std::vector<bool> in_use_{};
    std::mutex mutex_{};
    std::atomic_uint slot_using_{};
    std::queue<recv_entry> queue_{};
    std::size_t slot_size_{SLOT_SIZE};

    bool await(unsigned char& info, std::uint16_t& slot, std::string& payload) {
        DVLOG_LP(log_trace) << "-- enter waiting REQUEST --";  //NOLINT

        while (true) {
            if (!queue_.empty() && slot_using_ < slot_size_) {
                auto entry = queue_.front();
                info = entry.info();
                slot = entry.slot();
                payload = entry.payload();
                queue_.pop();
                return true;
            }

            struct timeval tv{};
            fd_set fds;
            FD_ZERO(&fds);  // NOLINT
            FD_SET(socket_, &fds);  // NOLINT
            tv.tv_sec = 0;
            tv.tv_usec = 500000;  // 500(mS)
            if (auto rv = select(socket_ + 1, &fds, nullptr, nullptr, &tv); rv == 0) {
                continue;
            }

            if (FD_ISSET(socket_, &fds)) {  // NOLINT
                if (auto size_i = ::recv(socket_, &info, 1, 0); size_i == 0) {
                    DVLOG_LP(log_trace) << "socket is closed by the client";  //NOLINT
                    return false;
                }

                char buffer[sizeof(std::uint16_t)];  // NOLINT
                if (!recv(&buffer[0], sizeof(std::uint16_t))) {
                        DVLOG_LP(log_trace) << "socket is closed by the client abnormally";  //NOLINT
                        return false;
                }
                slot = (strip(buffer[1]) << 8) | strip(buffer[0]);  // NOLINT
            }
            switch (info) {
            case REQUEST_SESSION_PAYLOAD:
                DVLOG_LP(log_trace) << "--> REQUEST_SESSION_PAYLOAD " << static_cast<std::uint32_t>(slot);  //NOLINT
                if (recv(payload)) {
                    if (slot_using_ < slot_size_) {
                        return true;
                    }
                    queue_.push(recv_entry(info, slot, payload));
                    break;
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";  //NOLINT
                return false;
            case REQUEST_RESULT_SET_BYE_OK:
            {
                DVLOG_LP(log_trace) << "--> REQUEST_RESULT_SET_BYE_OK " << static_cast<std::uint32_t>(slot);  //NOLINT
                std::string dummy;
                if (recv(dummy)) {
                    release_slot(slot);
                    break;
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";  //NOLINT
                return false;
            }
            case REQUEST_SESSION_HELLO:
                DVLOG_LP(log_trace) << "--> REQUEST_SESSION_HELLO ";  //NOLINT
                if (recv(payload)) {
                    return true;  // supposed to return to stream_socket()
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";  //NOLINT
                return false;
            case REQUEST_SESSION_BYE:
                DVLOG_LP(log_trace) << "--> REQUEST_SESSION_BYE ";  //NOLINT
                if (recv(payload)) {
                    do {std::unique_lock<std::mutex> lock(mutex_);
                        session_closed_ = true;
                    } while (false);
                    DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_BYE_OK ";  //NOLINT
                    send_response(RESPONSE_SESSION_BYE_OK, 0, "", true);
                    continue;
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";  //NOLINT
                return false;
            default:
                LOG_LP(ERROR) << "illegal message type " << static_cast<std::uint32_t>(info);  //NOLINT
                return false;  // to exit this thread
            }
        }
    }
    std::size_t strip(char c) {
        return (static_cast<std::uint32_t>(c) & 0xff);  // NOLINT
    }

    [[nodiscard]] bool recv(std::string& payload) {
        char buffer[4];  // NOLINT
        if (!recv(&buffer[0], sizeof(int))) {
            return false;
        }
        unsigned int length = (strip(buffer[3]) << 24) | (strip(buffer[2]) << 16) | (strip(buffer[1]) << 8) | strip(buffer[0]);  // NOLINT

        if (length > 0) {
            payload.resize(length);
            char *data_buffer = payload.data();
            if (!recv(data_buffer, length)) {
                return false;
            }
        }
        return true;
    }
    [[nodiscard]] bool recv(char* to, const std::size_t size) {  // NOLINT
        auto received = ::recv(socket_, to, size, 0);
        if (received == 0) {
            return false;
        }
        while (received < static_cast<std::int64_t>(size)) {
            auto s = ::recv(socket_, to + received, size - received, 0);  // NOLINT
            if (s == 0) {
                return false;
            }
            received += s;
        }
        return true;
    }

    void send_response(unsigned char info, std::uint16_t slot, std::string_view payload, bool force = false) {  // a support function, assumes caller hold lock
        char buffer[sizeof(std::uint16_t)];  // NOLINT
        std::unique_lock<std::mutex> lock(mutex_);
        if (session_closed_ && !force) {
            return;
        }
        ::send(socket_, &info, 1, 0);
        buffer[0] = slot & 0xff;  // NOLINT
        buffer[1] = (slot / 0x100) & 0xff;  // NOLINT
        ::send(socket_, &buffer[0], sizeof(std::uint16_t), 0);
        send_payload(payload);
    }
    void send_payload(std::string_view payload) const {  // a support function, assumes caller hold lock
        unsigned int length = payload.length();
        char buffer[4];  // NOLINT
        buffer[0] = length & 0xff;  // NOLINT
        buffer[1] = (length / 0x100) & 0xff;  // NOLINT
        buffer[2] = (length / 0x10000) & 0xff;  // NOLINT
        buffer[3] = (length / 0x1000000) & 0xff;  // NOLINT
        ::send(socket_, &buffer[0], sizeof(length), 0);
        if (length > 0) {
            ::send(socket_, payload.data(), length, 0);
        }
    }

    void release_slot(unsigned int slot) {
        if (!in_use_.at(slot)) {
            LOG_LP(ERROR) << "slot " << slot << " is not using";
            return;
        }
        in_use_.at(slot) = false;
        slot_using_--;
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
            throw std::runtime_error("cannot create a pipe");  //NOLINT
        }

        // create a socket
        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        const int enable = 1;
        if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
            LOG_LP(ERROR) << "setsockopt() fail";
        }

        // Map the address and the port to the socket
        struct sockaddr_in socket_address{};
        socket_address.sin_family = AF_INET;
        socket_address.sin_port = htons(port);
        socket_address.sin_addr.s_addr = INADDR_ANY;
        if (bind(socket_, (struct sockaddr *) &socket_address, sizeof(socket_address)) != 0) {  // NOLINT
            throw std::runtime_error("bind error, probably another server is running on the same port");  //NOLINT
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
                throw std::runtime_error("pipe connection error");  //NOLINT
            }
            return nullptr;
        }
        throw std::runtime_error("select error");  //NOLINT
    }

    void request_terminate() {
        if (write(pair_[1], "q", 1) <= 0) {
            LOG_LP(ERROR) << "fail to request terminate";
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
