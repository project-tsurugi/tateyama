/*
 * Copyright 2025-2025 Project Tsurugi.
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
#include <thread>

#include <tateyama/system/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/system/request.pb.h>
#include <tateyama/proto/system/response.pb.h>
#include <tateyama/proto/system/diagnostic.pb.h>

namespace tateyama::system::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief Handles a system service request.
 *
 * This function processes the incoming request and populates the response accordingly.
 *
 * @param req The incoming request object.
 * @param res The response object to populate.
 * @return true if the request was handled successfully; false otherwise.
 *
 * @note
 * Any exceptions of type std::runtime_error thrown when a permission error occurs
 * during request processing are caught internally. In such cases, an error response
 * is set in @p res, and the function returns false. Callers should not expect
 * std::runtime_error exceptions to propagate from this function.
 */
bool system_service_core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    try {
        tateyama::proto::system::request::Request rq{};

        auto data = req->payload();
        if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            LOG_LP(ERROR) << "request parse error";
            return false;
        }

        if (rq.service_message_version_major() > 0 ||
            (rq.service_message_version_major() == 0 && rq.service_message_version_minor() > 0)) {
            tateyama::proto::diagnostics::Record error{};
            error.set_code(tateyama::proto::diagnostics::Code::INVALID_REQUEST);
            error.set_message("unsupported message version");
            res->error(error);
            return false;
        }

        auto session_id = req->session_id();
        res->session_id(session_id);
        switch(rq.command_case()) {
        case tateyama::proto::system::request::Request::kGetSystemInfo:
        {
            tateyama::proto::system::response::GetSystemInfo rs{};
            if (!info_handler_.error()) {
                info_handler_.set_system_info(rs.mutable_success()->mutable_system_info());
                res->body(rs.SerializeAsString());
                return true;
            }
            auto* error = rs.mutable_error();
            error->set_code(tateyama::proto::system::diagnostic::ErrorCode::NOT_FOUND);
            error->set_message("cannot find system information");
            res->body(rs.SerializeAsString());
            return true;
        }

        default:
            return false;
        }

    } catch (const std::runtime_error &ex) {
        tateyama::proto::diagnostics::Record error{};
        error.set_code(tateyama::proto::diagnostics::Code::UNKNOWN);
        error.set_message(ex.what());
        res->error(error);
        return false;
    }
}

system_service_core::system_service_core() = default;

}
