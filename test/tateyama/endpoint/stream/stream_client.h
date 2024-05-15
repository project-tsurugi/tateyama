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

#include <tateyama/framework/component_ids.h>
#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

namespace tateyama::api::endpoint::stream {

class stream_client {
    static constexpr std::uint8_t REQUEST_SESSION_PAYLOAD = 2;
    static constexpr std::uint8_t REQUEST_RESULT_SET_BYE_OK = 3;
    static constexpr std::uint8_t REQUEST_SESSION_BYE = 4;

    static constexpr std::uint8_t RESPONSE_SESSION_PAYLOAD = 1;
    static constexpr std::uint8_t RESPONSE_RESULT_SET_PAYLOAD = 2;
    static constexpr std::uint8_t RESPONSE_RESULT_SET_HELLO = 5;
    static constexpr std::uint8_t RESPONSE_RESULT_SET_BYE = 6;
    static constexpr std::uint8_t RESPONSE_SESSION_BODYHEAD = 7;

public:
    static constexpr int PORT_FOR_TEST = 12349;
    static constexpr int TERMINATION_REQUEST = 0xffff;

    explicit stream_client(tateyama::proto::endpoint::request::Handshake& hs);
    stream_client() : stream_client(default_endpoint_handshake_) {
    }
    ~stream_client() {
        ::close(sockfd_);
    }

    bool send(const std::uint8_t type, const std::uint16_t slot, std::string_view message);
    bool send(const std::size_t tag, std::string_view message, std::size_t index_offset = 0);
    void receive(std::string &message);
    void receive() { receive(response_); }
    void receive(std::string &message, tateyama::proto::framework::response::Header::PayloadType& type);
    void disconnect() {
        if (!send_bye_) {
            send(REQUEST_SESSION_BYE, TERMINATION_REQUEST, "");
            send_bye_ = true;
            std::string dmy{};
            receive(dmy);
        }
    }
    void close() {
        disconnect();
        ::close(sockfd_);
    }

    std::string& handshake_response() { return handshake_response_; }
    std::string& response() { return response_; }

    std::uint8_t type_{};
    std::uint16_t slot_{};
    std::uint8_t writer_{};

private:
    tateyama::proto::endpoint::request::Handshake& endpoint_handshake_;
    int sockfd_{};
    tateyama::proto::endpoint::request::Handshake default_endpoint_handshake_{};
    std::string handshake_response_{};
    std::string response_{};
    bool send_bye_{};

    void handshake();
    void receive(std::string &message, tateyama::proto::framework::response::Header::PayloadType type, bool do_check);
};

} // namespace tateyama::api::endpoint::stream
