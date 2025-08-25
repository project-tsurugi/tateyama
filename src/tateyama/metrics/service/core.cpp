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
#include <tateyama/metrics/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/metrics/request.pb.h>
#include <tateyama/proto/metrics/response.pb.h>

namespace tateyama::metrics::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

__thread bool ipc_correction{};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool tateyama::metrics::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    if (req->session_info().user_type() != tateyama::api::server::user_type::administrator) {
        tateyama::proto::diagnostics::Record error{};
        error.set_code(tateyama::proto::diagnostics::Code::PERMISSION_ERROR);
        error.set_message("administrator privilege is required");
        res->error(error);
        return false;
    }

    tateyama::proto::metrics::request::Request rq{};
    auto data = req->payload();
    if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        LOG(ERROR) << "request parse error";
        return false;
    }

    tateyama::proto::metrics::response::MetricsInformation rs{};
    switch(rq.command_case()) {
    case tateyama::proto::metrics::request::Request::kList:
        resource_->core().list(rs);
        res->body(rs.SerializeAsString());
        break;

    case tateyama::proto::metrics::request::Request::kShow:
        ipc_correction = (req->session_info().connection_type_name() == "ipc");
        resource_->core().show(rs);
        res->body(rs.SerializeAsString());
        ipc_correction = false;
        break;

    case tateyama::proto::metrics::request::Request::COMMAND_NOT_SET:
        return false;
    }

    return true;
}

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{}

bool core::start(tateyama::metrics::resource::bridge* resource) {
    resource_ = resource;
    return true;
}

bool core::shutdown(bool force) {
    //TODO implement
    (void) force;
    return true;
}

}
