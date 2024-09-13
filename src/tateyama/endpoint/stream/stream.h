/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <netinet/tcp.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/poll.h>
#include <memory>
#include <string_view>
#include <stdexcept>
#include <vector>
#include <iterator>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <chrono>
#include <functional>
#include <arpa/inet.h>


#include <glog/logging.h>

#include <tateyama/logging.h>
#include "tateyama/logging_helper.h"

namespace tateyama::endpoint::stream {

class connection_socket;
class stream_data_channel;
class connection_socket;

class stream_socket
{
    static constexpr unsigned char REQUEST_SESSION_HELLO = 1;      // for backward compatibility
    static constexpr unsigned char REQUEST_SESSION_PAYLOAD = 2;
    static constexpr unsigned char REQUEST_RESULT_SET_BYE_OK = 3;
    static constexpr unsigned char REQUEST_SESSION_BYE = 4;

    static constexpr unsigned char RESPONSE_SESSION_PAYLOAD = 1;
    static constexpr unsigned char RESPONSE_RESULT_SET_PAYLOAD = 2;
    static constexpr unsigned char RESPONSE_SESSION_HELLO_OK = 3;  // for backward compatibility
    // 4 is nolonger used
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
    enum class await_result : std::uint32_t {
        /**
         * @brief received a request message.
         */
        payload = 0U,

        /**
         * @brief timeout occurred while waiting for a request message.
         */
        timeout,

        /**
         * @brief the socket has been closed by the client, means client has requested shutdown this session.
         */
        socket_closed,

        /**
         * @brief the client has sent a request of session close.
         */
        termination_request,

        /**
         * @brief the message received is in an illegal format.
         */
        illegal_message,
    };

    explicit stream_socket(int socket, std::string_view info, connection_socket* envelope);
    ~stream_socket();

    stream_socket(stream_socket const& other) = delete;
    stream_socket& operator=(stream_socket const& other) = delete;
    stream_socket(stream_socket&& other) noexcept = delete;
    stream_socket& operator=(stream_socket&& other) noexcept = delete;

    [[nodiscard]] await_result await(std::uint16_t& slot, std::string& payload) {
        unsigned char info{};
        return await(info, slot, payload);
    }

    void send(std::uint16_t slot, std::string_view payload, bool body) {  // for RESPONSE_SESSION_PAYLOAD
        if (body) {
            DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_PAYLOAD " << static_cast<std::uint32_t>(slot);
            send_response(RESPONSE_SESSION_PAYLOAD, slot, payload);
        } else {
            DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_BODYHEAD " << static_cast<std::uint32_t>(slot);
            send_response(RESPONSE_SESSION_BODYHEAD, slot, payload);
        }
    }
    void send_result_set_hello(std::uint16_t slot, std::string_view name) {  // for RESPONSE_RESULT_SET_HELLO
        DVLOG_LP(log_trace)  << "<-- RESPONSE_RESULT_SET_HELLO " << static_cast<std::uint32_t>(slot) << ", " << name;
        send_response(RESPONSE_RESULT_SET_HELLO, slot, name);
    }
    void send_result_set_bye(std::uint16_t slot) {  // for RESPONSE_RESULT_SET_BYE
        DVLOG_LP(log_trace) << "<-- RESPONSE_RESULT_SET_BYE " << static_cast<std::uint32_t>(slot);
        send_response(RESPONSE_RESULT_SET_BYE, slot, "");
    }
    void send_session_bye_ok() {  // for RESPONSE_SESSION_BYE_OK
        DVLOG_LP(log_trace) << "<-- RESPONSE_SESSION_BYE_OK ";
        send_response(RESPONSE_SESSION_BYE_OK, 0, "", true);
    }

    void send(std::uint16_t slot, unsigned char writer, std::string_view payload) { // for RESPONSE_RESULT_SET_PAYLOAD
        DVLOG_LP(log_trace) << (payload.length() > 0 ? "<-- RESPONSE_RESULT_SET_PAYLOAD " : "<-- RESPONSE_RESULT_SET_COMMIT ") << static_cast<std::uint32_t>(slot) << ", " << static_cast<std::uint32_t>(writer);
        std::unique_lock<std::mutex> lock(mutex_);
        if (session_closed_) {
            return;
        }
        unsigned char info = RESPONSE_RESULT_SET_PAYLOAD;
        ::send(socket_, &info, 1, MSG_NOSIGNAL);
        char buffer[sizeof(std::uint16_t)];  // NOLINT
        buffer[0] = slot & 0xff;  // NOLINT
        buffer[1] = (slot / 0x100) & 0xff;  // NOLINT
        ::send(socket_, &buffer[0], sizeof(std::uint16_t), MSG_MORE | MSG_NOSIGNAL);
        ::send(socket_, &writer, 1, MSG_MORE | MSG_NOSIGNAL);
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
        std::unique_lock<std::mutex> lock(slot_mutex_);
        for (unsigned int i = 0; i < slot_size_; i++) {  // FIXME more efficient
            if (!in_use_.at(i)) {
                in_use_.at(i) = true;
                slot_using_++;
                return i;
            }
        }
        throw std::runtime_error("running out the slots for result set");
    }

    void change_slot_size(std::size_t index) {
        std::unique_lock<std::mutex> lock(slot_mutex_);
        if ((index + 1) > slot_size_) {
            DVLOG_LP(log_trace) << "enlarge srot to " << (index + 1);
            slot_size_ = index + 1;
            in_use_.resize(slot_size_);
        }
    }

    [[nodiscard]] std::string_view connection_info() const {
        return connection_info_;
    }

    bool set_owner(void* owner) {
        return owner_.exchange(owner, std::memory_order_acquire) == nullptr;
    }

private:
    int socket_;
    static constexpr std::size_t N_FDS = 1;
    static constexpr int TIMEOUT_MS = 2000;  // 2000(mS)
    struct pollfd fds_[N_FDS]{};  // NOLINT

    bool session_closed_{false};
    bool socket_closed_{false};
    std::vector<bool> in_use_{};
    std::atomic_uint slot_using_{};
    std::queue<recv_entry> queue_{};
    std::size_t slot_size_{SLOT_SIZE};
    std::string connection_info_{};
    std::mutex mutex_{};
    std::mutex slot_mutex_{};
    std::atomic<void*> owner_{nullptr};
    connection_socket* envelope_;

    await_result await(unsigned char& info, std::uint16_t& slot, std::string& payload) {
        fds_[0].fd = socket_;               // NOLINT
        fds_[0].events = POLLIN | POLLPRI;  // NOLINT
        fds_[0].revents = 0;                // NOLINT
        while (true) {
            if (!queue_.empty() && slot_using_ < slot_size_) {
                auto entry = queue_.front();
                info = entry.info();
                slot = entry.slot();
                payload = entry.payload();
                queue_.pop();
                return await_result::payload;
            }

            if (auto rv = poll(fds_, N_FDS, TIMEOUT_MS); !(rv > 0)) {  // NOLINT
                if (rv == 0) {
                    return await_result::timeout;
                }
                throw std::runtime_error("error in poll");
            }

            if (fds_[0].revents & POLLPRI) {  // NOLINT
                unsigned char buf{};
                if (::recv (socket_, &buf, 1, MSG_OOB) < 0) {
                    return await_result::socket_closed;
                }
                return await_result::timeout;
            }
            if (fds_[0].revents & POLLIN) {  // NOLINT
                if (auto size_i = ::recv(socket_, &info, 1, 0); size_i == 0) {
                    DVLOG_LP(log_trace) << "socket is closed by the client";
                    return await_result::socket_closed;
                }

                char buffer[sizeof(std::uint16_t)];  // NOLINT
                if (!recv(&buffer[0], sizeof(std::uint16_t))) {
                        DVLOG_LP(log_trace) << "socket is closed by the client abnormally";
                        return await_result::socket_closed;
                }
                slot = (strip(buffer[1]) << 8) | strip(buffer[0]);  // NOLINT
            }
            switch (info) {
            case REQUEST_SESSION_PAYLOAD:
                DVLOG_LP(log_trace) << "--> REQUEST_SESSION_PAYLOAD " << static_cast<std::uint32_t>(slot);
                if (recv(payload)) {
                    if (slot_using_ < slot_size_) {
                        return await_result::payload;
                    }
                    queue_.push(recv_entry(info, slot, payload));
                    break;
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";
                return await_result::socket_closed;
            case REQUEST_RESULT_SET_BYE_OK:
            {
                DVLOG_LP(log_trace) << "--> REQUEST_RESULT_SET_BYE_OK " << static_cast<std::uint32_t>(slot);
                std::string dummy;
                if (recv(dummy)) {
                    release_slot(slot);
                    break;
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";
                return await_result::socket_closed;
            }
            case REQUEST_SESSION_HELLO:  // for backward compatibility
                if (recv(payload)) {
                    std::string session_name = std::to_string(-1);  // dummy as this session will be closed soon.
                    send_response(RESPONSE_SESSION_HELLO_OK, 0, session_name);
                } else {
                    DVLOG_LP(log_trace) << "socket is closed by the client abnormally";
                }
                continue;
            case REQUEST_SESSION_BYE:
                DVLOG_LP(log_trace) << "--> REQUEST_SESSION_BYE ";
                if (recv(payload)) {
                    do {std::unique_lock<std::mutex> lock(mutex_);
                        session_closed_ = true;
                    } while (false);
                    return await_result::termination_request;
                }
                DVLOG_LP(log_trace) << "socket is closed by the client abnormally";
                return await_result::socket_closed;
            default:
                LOG_LP(ERROR) << "illegal message type " << static_cast<std::uint32_t>(info);
                close();
                return await_result::illegal_message;  // to exit this thread
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
        ::send(socket_, &info, 1, MSG_NOSIGNAL);
        buffer[0] = slot & 0xff;  // NOLINT
        buffer[1] = (slot / 0x100) & 0xff;  // NOLINT
        ::send(socket_, &buffer[0], sizeof(std::uint16_t), MSG_MORE | MSG_NOSIGNAL);
        send_payload(payload);
    }
    void send_payload(std::string_view payload) const {  // a support function, assumes caller hold lock
        unsigned int length = payload.length();
        char buffer[4];  // NOLINT
        buffer[0] = length & 0xff;  // NOLINT
        buffer[1] = (length / 0x100) & 0xff;  // NOLINT
        buffer[2] = (length / 0x10000) & 0xff;  // NOLINT
        buffer[3] = (length / 0x1000000) & 0xff;  // NOLINT
        if (length > 0) {
            ::send(socket_, &buffer[0], sizeof(length), MSG_MORE | MSG_NOSIGNAL);
            ::send(socket_, payload.data(), length, MSG_NOSIGNAL);
        } else {
            ::send(socket_, &buffer[0], sizeof(length), MSG_NOSIGNAL);
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
// one object per one stream endpoint
class connection_socket
{
    static constexpr std::size_t default_socket_limit = 16384;

public:
    /**
     * @brief Construct a new object.
     */
    connection_socket() = delete;
    connection_socket(std::uint32_t port, std::size_t timeout, std::size_t socket_limit) : socket_limit_(socket_limit), timeout_(timeout) {
        // create a pipe
        if (pipe(&pair_[0]) != 0) {
            throw std::runtime_error("cannot create a pipe");
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
            throw std::runtime_error("bind error, probably another server is running on the same port");
        }
        // listen the port
        listen(socket_, SOMAXCONN);
    }
    connection_socket(std::uint32_t port, std::size_t timeout) :  connection_socket(port, timeout, default_socket_limit) {}
    explicit connection_socket(std::uint32_t port) :  connection_socket(port, 1000, default_socket_limit) {}  // for tests
    ~connection_socket() = default;

    /**
     * @brief Copy and move constructers are deleted.
     */
    connection_socket(connection_socket const&) = delete;
    connection_socket(connection_socket&&) = delete;
    connection_socket& operator = (connection_socket const&) = delete;
    connection_socket& operator = (connection_socket&&) = delete;

    std::shared_ptr<stream_socket> accept(const std::function<void(void)>& cleanup = [](){} ) {
        cleanup();

        while (true) {
            struct timeval tv{};
            tv.tv_sec = static_cast<std::int64_t>(timeout_ / 1000);            // sec
            tv.tv_usec = static_cast<std::int64_t>((timeout_ % 1000) * 1000);  // usec
            FD_ZERO(&fds_);  // NOLINT
            if (is_socket_available()) {
                FD_SET(socket_, &fds_);  // NOLINT
            }
            FD_SET(pair_[0], &fds_);  // NOLINT
            if (auto rv = select(((pair_[0] > socket_) ? pair_[0] : socket_) + 1, &fds_, nullptr, nullptr, &tv); rv == 0) {
                cleanup();
                continue;
            }
            if (FD_ISSET(socket_, &fds_)) {  // NOLINT
                // Accept a connection request
                struct sockaddr_in address{};
                unsigned int len = sizeof(address);
                int ts = ::accept(socket_, (struct sockaddr *)&address, &len);  // NOLINT
                if (ts == -1) {
                    throw std::runtime_error("accept error");
                }
                std::stringstream ss{};
                ss << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port);
                return std::make_shared<stream_socket>(ts, ss.str(), this);
            }
            if (FD_ISSET(pair_[0], &fds_)) {  //  NOLINT
                char trash{};
                if (read(pair_[0], &trash, sizeof(trash)) <= 0) {
                    throw std::runtime_error("pipe connection error");
                }
                return nullptr;
            }
            throw std::runtime_error("select error");
        }
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
    fd_set fds_{};

    std::size_t socket_limit_;
    std::atomic_uint64_t num_open_{0};
    std::mutex num_mutex_{};
    std::condition_variable num_condition_{};
    std::size_t timeout_;

    friend class stream_socket;

    [[nodiscard]] bool is_socket_available() {
        return num_open_.load() < socket_limit_;
    }
    bool wait_socket_available() {
        std::unique_lock<std::mutex> lock(num_mutex_);
        return num_condition_.wait_for(lock, std::chrono::seconds(5), [this](){ return is_socket_available(); });
    }
    void notify_of_close() {
        std::unique_lock<std::mutex> lock(num_mutex_);
        num_condition_.notify_one();
    }
};

}
