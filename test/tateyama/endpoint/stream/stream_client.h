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

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>

namespace tateyama::api::endpoint::stream {

class stream_client {
    static constexpr std::uint8_t REQUEST_SESSION_HELLO = 1;
    static constexpr std::uint8_t REQUEST_SESSION_PAYLOAD = 2;
    static constexpr std::uint8_t REQUEST_RESULT_SET_BYE_OK = 3;
    static constexpr std::uint8_t REQUEST_SESSION_BYE = 4;

    static constexpr std::uint8_t RESPONSE_SESSION_PAYLOAD = 1;
    static constexpr std::uint8_t RESPONSE_RESULT_SET_PAYLOAD = 2;
    static constexpr std::uint8_t RESPONSE_SESSION_HELLO_OK = 3;
    static constexpr std::uint8_t RESPONSE_SESSION_HELLO_NG = 4;
    static constexpr std::uint8_t RESPONSE_RESULT_SET_HELLO = 5;
    static constexpr std::uint8_t RESPONSE_RESULT_SET_BYE = 6;
    static constexpr std::uint8_t RESPONSE_SESSION_BODYHEAD = 7;

public:
    static constexpr int PORT_FOR_TEST = 12349;

    stream_client();
    ~stream_client() {
        ::close(sockfd_);
    }

    void send(const std::uint8_t type, const std::uint16_t slot, std::string_view message);
    void send(std::string_view message);
    void receive();
    void close() {
        ::close(sockfd_);
    }

    std::uint8_t type_{};
    std::uint16_t slot_{};
    std::uint8_t writer_{};
    std::string message_{};

private:
    int sockfd_;
    ::tateyama::proto::framework::request::Header header_{};
};

} // namespace tateyama::api::endpoint::stream
