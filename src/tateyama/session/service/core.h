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

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/session/resource/bridge.h>

namespace tateyama::session::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief session service main object
 */
class core {
public:
    core() = default;

    explicit core(std::shared_ptr<tateyama::api::configuration::whole> cfg);

    bool start(tateyama::session::resource::bridge* resource);

    bool shutdown(bool force = false);

    bool operator()(
        const std::shared_ptr<request>& req,
        const std::shared_ptr<response>& res
    );

private:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
    tateyama::session::resource::bridge* resource_{};

    template<typename T> 
    void send_error(
        const std::shared_ptr<response>& res,
        tateyama::proto::session::diagnostic::ErrorCode err = tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN
    ) {
        T rs{};
        auto* error = rs.mutable_error();
        error->set_error_code(err);
        res->body(rs.SerializeAsString());
        rs.clear_error();
    }
};

}
