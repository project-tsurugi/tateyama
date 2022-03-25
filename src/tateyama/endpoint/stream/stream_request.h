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

#include <string_view>

#include <tateyama/api/endpoint/request.h>

#include "stream.h"

namespace tateyama::common::stream {

/**
 * @brief request object for stream_endpoint
 */
class stream_request : public tateyama::api::endpoint::request {
public:
    stream_request() = delete;
    explicit stream_request(stream_socket& session_socket) : session_socket_(session_socket) {
        session_socket_.recv(payload_);
    }

    [[nodiscard]] std::string_view payload() const override;
    stream_socket& get_session_socket() { return session_socket_; }
    
private:
    stream_socket& session_socket_;
    std::string payload_;
};

}  // tateyama::common::stream
