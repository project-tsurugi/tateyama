/*
 * Copyright 2019-2021 tsurugi project.
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

#include <tateyama/api/server/request.h>

#include "server_wires.h"
#include "../common/endpoint_proto_utils.h"

namespace tateyama::common::wire {

/**
 * @brief request object for ipc_endpoint
 */
class ipc_request : public tateyama::api::server::request {
public:
    ipc_request(server_wire_container& server_wire, message_header& header)
        : server_wire_(server_wire), length_(header.get_length()), read_point(server_wire_.get_request_wire()->read_point()) {
        auto message = server_wire_.get_request_wire()->payload();
        endpoint::common::parse_result res{};
        endpoint::common::parse_header(message, res); // TODO handle error
        payload_ = res.payload_;
        session_id_ = res.session_id_;
        service_id_ = res.service_id_;
    }

    ipc_request() = delete;

    [[nodiscard]] std::string_view payload() const override;
    server_wire_container& get_server_wire_container();
    void dispose();
    [[nodiscard]] std::size_t session_id() const override;
    [[nodiscard]] std::size_t service_id() const override;
private:
    server_wire_container& server_wire_;
    const std::size_t length_;
    const std::size_t read_point;
    std::size_t session_id_{};
    std::size_t service_id_{};
    std::string_view payload_{};
};

}  // tateyama::common::wire
