/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <memory>

#include <limestone/api/datastore.h>
#include <tateyama/datastore/resource/bridge.h>

#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include <tateyama/framework/service.h>

namespace tateyama::blob_relay_privilege::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief blob_relay_privilege service main object
 */
class core {
public:
    core() = default;

    explicit core(tateyama::framework::environment& env);

    bool start(tateyama::framework::environment& env);

    bool operator()(
        const std::shared_ptr<request>& req,
        const std::shared_ptr<response>& res
    );

private:
    bool activated_{};
    std::shared_ptr<::tateyama::datastore::resource::bridge> datastore_resource_{};

    constexpr static uint64_t LIMESTONE_BLOB_STORE = 1;

    template<typename T>
    void send_error(
        const std::shared_ptr<response>& res,
        const tateyama::proto::diagnostics::Code err_code,
        const std::string& err_msg = "unknown"
    ) {
        T rs{};
        auto* error = rs.mutable_error();
        error->set_code(err_code);
        error->set_message(err_msg);
        res->body(rs.SerializeAsString());
        rs.clear_error();
    }
    void send_diagnostics(
        const std::shared_ptr<response>& res,
        const tateyama::proto::diagnostics::Code err_code,
        const std::string& err_msg = "unknown"
    ) {
        tateyama::proto::diagnostics::Record error{};
        error.set_code(err_code);
        error.set_message(err_msg);
        res->error(error);
    }
};

}
