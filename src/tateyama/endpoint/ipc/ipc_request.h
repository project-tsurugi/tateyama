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
#include <array>

#include <tateyama/api/server/request.h>

#include "server_wires.h"
#include "../common/endpoint_proto_utils.h"

namespace tateyama::common::wire {

/**
 * @brief request object for ipc_endpoint
 */
class ipc_request : public tateyama::api::server::request {
    constexpr static std::size_t SPO_SIZE = 256;

public:
    ipc_request(server_wire_container& server_wire, message_header& header)
        : server_wire_(server_wire), length_(header.get_length()) {
        std::string_view message{};
        auto *request_wire = server_wire_.get_request_wire();

        read_point_ = request_wire->read_point();
        if (length_ <= SPO_SIZE) {
            request_wire->read(spo_.data());
            message = std::string_view(spo_.data(), length_);
        } else {
            long_payload_.resize(length_);
            request_wire->read(long_payload_.data());
            message = std::string_view(long_payload_.data(), length_);
        }
        endpoint::common::parse_result res{};
        endpoint::common::parse_header(message, res); // TODO handle error
        payload_ = res.payload_;
        session_id_ = res.session_id_;
        service_id_ = res.service_id_;
        request_wire->dispose(read_point_);
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
    std::size_t read_point_;
    std::size_t session_id_{};
    std::size_t service_id_{};
    std::string_view payload_{};
    std::array<char, SPO_SIZE> spo_{};
    std::string long_payload_{};
};

}  // tateyama::common::wire
