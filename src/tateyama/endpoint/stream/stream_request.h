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

#include <string_view>

#include <tateyama/api/server/request.h>

#include "stream.h"
#include "tateyama/endpoint/common/endpoint_proto_utils.h"

namespace tateyama::common::stream {

/**
 * @brief request object for stream_endpoint
 */
class stream_request : public tateyama::api::server::request {
public:
    stream_request() = delete;
    explicit stream_request(stream_socket& session_socket, std::string& payload, const tateyama::api::server::database_info& database_info, const tateyama::api::server::session_info& session_info)
        : session_socket_(session_socket), database_info_(database_info), session_info_(session_info) {
        endpoint::common::parse_result res{};
        endpoint::common::parse_header(payload, res); // TODO handle error
        payload_ = res.payload_;
        session_id_ = res.session_id_;
        service_id_ = res.service_id_;
    }

    [[nodiscard]] std::string_view payload() const override;
    [[nodiscard]] std::size_t session_id() const override;
    [[nodiscard]] std::size_t service_id() const override;

    tateyama::api::server::database_info const& database_info() const noexcept override;
    tateyama::api::server::session_info const& session_info() const noexcept override;

private:
    stream_socket& session_socket_;
    const tateyama::api::server::database_info& database_info_;
    const tateyama::api::server::session_info& session_info_;

    std::string_view payload_{};
    std::size_t session_id_{};
    std::size_t service_id_{};
};

}  // tateyama::common::stream
