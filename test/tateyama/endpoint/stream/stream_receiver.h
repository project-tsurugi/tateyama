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

#include <cstdint>
#include <string>
#include <stdexcept>
#include <strings.h>  // for bzero()
#include <mutex>
#include <condition_variable>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace tateyama::endpoint::stream {

class stream_receiver {
public:
    static constexpr std::uint8_t RESPONSE_RESULT_SET_PAYLOAD = 2;

    explicit stream_receiver(int port) {
        if ((sockfd_ = ::socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            throw std::runtime_error("error in create socket");
        }

        struct sockaddr_in client_addr;
        bzero((char *)&client_addr, sizeof(client_addr));
        client_addr.sin_family = PF_INET;
        client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        client_addr.sin_port = htons(port);

        if (connect(sockfd_, (struct sockaddr *)&client_addr, sizeof(client_addr)) > 0) {
            ::close(sockfd_);
            throw std::runtime_error("connect error");
        }
    }
    ~stream_receiver() {
        ::close(sockfd_);
    }
    void operator()() {
        bool receive_result{};
        do {
            receive_result = receive();
            notify();
        } while (receive_result);
    }
    void close() {
        ::close(sockfd_);
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        cnd_.wait(lock, [this]{ return received_; });
    }
    std::string& message() {
        received_ = false;
        return message_;
    }
    std::uint8_t type() {
        received_ = false;
        auto rv = type_;
        type_ = 0xff;
        return rv;
    }
    std::uint16_t slot() {
        received_ = false;
        auto rv = slot_;
        slot_ = 0xffff;
        return rv;
    }
    std::uint16_t writer() {
        received_ = false;
        auto rv = writer_;
        writer_ = 0xff;
        return rv;
    }

    std::string local_addr() {
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (int rc = getsockname(sockfd_, (struct sockaddr*)&sin, &len); rc != 0) {
            // error
            return "";
        }
        std::string host(inet_ntoa(sin.sin_addr));
        int port = ntohs(sin.sin_port);
        return host + ':' + std::to_string(port);
    }

private:
    int sockfd_{};
    std::string response_{};
    std::uint8_t type_{0xff};
    std::uint16_t slot_{0xffff};
    std::uint8_t writer_{0xff};
    std::string message_{};
    bool received_{};
    std::mutex mtx_{};
    std::condition_variable cnd_{};

    bool receive() {
        std::uint8_t data[4];  // NOLINT

        auto rv = ::recv(sockfd_, &type_, 1, 0);
        if (rv == 0) {
            return false;  // the server has closed the socket 
        }
        if (rv < 0) {
            throw std::runtime_error("error in recv()");
        }

        if (::recv(sockfd_, data, 2, 0) < 0) {
            throw std::runtime_error("error in recv()");
        }
        slot_ = data[0] | (data[1] << 8);

        if (type_ == RESPONSE_RESULT_SET_PAYLOAD) {
            if (::recv(sockfd_, &writer_, 1, 0) < 0) {
                throw std::runtime_error("error in recv()");
            }
        }

        if (::recv(sockfd_, data, 4, 0) < 0) {
            throw std::runtime_error("error in recv()");
        }
        std::size_t length = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

        if (length > 0) {
            message_.resize(length);
            if (::recv(sockfd_, message_.data(), length, 0) < 0) {
                throw std::runtime_error("error in recv()");
            }
        } else {
            message_.clear();
        }
        return true;
    }
    void notify() {
        std::unique_lock<std::mutex> lock(mtx_);
        received_ = true;
        cnd_.notify_one();
    }
};

} // namespace tateyama::api::endpoint::stream
