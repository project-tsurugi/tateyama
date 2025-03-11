/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <tateyama/endpoint/common/request.h>

#include "stream.h"

namespace tateyama::endpoint::stream {

/**
 * @brief request object for stream_endpoint
 */
class alignas(64) stream_request : public tateyama::endpoint::common::request {
public:
    stream_request() = delete;
    explicit stream_request(stream_socket& session_socket,
                            std::string& payload,
                            tateyama::endpoint::common::resources& resources,
                            std::size_t local_id,
                            const tateyama::endpoint::common::configuration& conf)
        : tateyama::endpoint::common::request(resources, local_id, conf), session_socket_(session_socket) {
        endpoint::common::parse_result res{};
        parse_framework_header(payload, res);
        payload_ = res.payload_;
    }

    [[nodiscard]] std::string_view payload() const override;

private:
    stream_socket& session_socket_;

    std::string payload_{};
};

}
