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
#include <array>
#include <exception>

#include <tateyama/endpoint/common/request.h>

#include "server_wires.h"
#include "tateyama/logging_helper.h"

namespace tateyama::endpoint::ipc {

/**
 * @brief request object for ipc_endpoint
 */
class alignas(64) ipc_request : public tateyama::endpoint::common::request {
    constexpr static std::size_t SPO_SIZE = 256;

public:
    ipc_request(server_wire_container& server_wire,
                tateyama::common::wire::message_header& header,
                tateyama::endpoint::common::resources& recources,
                std::size_t local_id,
                const tateyama::endpoint::common::configuration& conf) :
        tateyama::endpoint::common::request(recources, local_id, conf),
        server_wire_(server_wire),
        length_(header.get_length()),
        index_(header.get_idx()) {

        std::string_view message{};
        auto *request_wire = server_wire_.get_request_wire();

        if (length_ <= SPO_SIZE) {
            request_wire->read(spo_.data());
            message = std::string_view(spo_.data(), length_);
        } else {
            long_payload_.resize(length_);
            request_wire->read(long_payload_.data());
            message = std::string_view(long_payload_.data(), length_);
        }
        endpoint::common::parse_result res{};
        parse_framework_header(message, res);
        payload_ = res.payload_;
        request_wire->dispose();
    }

    ipc_request() = delete;

    [[nodiscard]] std::string_view payload() const override;
    void dispose();

private:
    server_wire_container& server_wire_;
    const std::size_t length_;
    const std::size_t index_;

    std::string payload_{};
    std::array<char, SPO_SIZE> spo_{};
    std::string long_payload_{};
};

}
