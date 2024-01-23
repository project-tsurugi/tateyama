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
#include <tateyama/framework/routing_service.h>

#include <functional>
#include <memory>
#include <type_traits>
#include <sstream>

#include <takatori/util/fail.h>

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>

#include <tateyama/proto/core/request.pb.h>
#include <tateyama/proto/core/response.pb.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;

component::id_type routing_service::id() const noexcept {
    return tag;
}

bool routing_service::setup(environment& env) {
    if(env.mode() == boot_mode::maintenance_standalone) {
        return true;
    }
    services_ = std::addressof(env.service_repository());
    return true;
}

bool routing_service::start(environment&) {
    //no-op
    return true;
}

bool routing_service::shutdown(environment&) {
    services_ = {};
    return true;
}

bool handle_update_expiration_time(
    std::shared_ptr<request> const& req,
    std::shared_ptr<response> const& res
) {
    namespace ns = proto::core::request;
    auto data = req->payload();
    ns::Request rq{};
    if(! rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        VLOG_LP(log_error) << "request parse error";
        return false;
    }
    if(rq.command_case() != ns::Request::kUpdateExpirationTime) {
        LOG_LP(ERROR) << "bad request destination (routing service): " << rq.command_case();
        return false;
    }
    if(! rq.has_update_expiration_time()) {
        LOG_LP(ERROR) << "bad request content";
        return false;
    }
    // mock impl. for UpdateExpirationTime // TODO
    auto et = rq.update_expiration_time().expiration_time();
    VLOG_LP(log_debug) <<
        "UpdateExpirationTime received session_id:" << req->session_id() <<
        " expiration_time:" << et;
    tateyama::proto::core::response::UpdateExpirationTime rp{};
    rp.mutable_success();
    res->session_id(req->session_id());
    auto body = rp.SerializeAsString();
    res->body(body);
    rp.clear_success();
    return true;
}

bool routing_service::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    if (services_ == nullptr) {
        LOG_LP(ERROR) << "routing service is not setup, or framework is running on standalone mode";
        return false;
    }
    if (req->service_id() == tag) {
        // must be UpdateExpirationTime
        return handle_update_expiration_time(req, res);
    }
    if (auto destination = services_->find_by_id(req->service_id()); destination != nullptr) {
        destination->operator()(std::move(req), std::move(res));
        return true;
    }

    std::stringstream ss{};
    ss << "unsupported service message: the destination service (ID=" << req->service_id() << ") is not registered." << std::endl <<
        "see https://github.com/project-tsurugi/tsurugidb/blob/master/docs/upgrade-guide.md#service-not-registered";
    auto msg = ss.str();
    ::tateyama::proto::diagnostics::Record record{};
    record.set_code(::tateyama::proto::diagnostics::Code::SERVICE_UNAVAILABLE);
    record.set_message(msg);
    res->error(record);
    VLOG_LP(log_info) << msg;
    return false;
}

std::string_view routing_service::label() const noexcept {
    return component_label;
}

routing_service::~routing_service() {
    VLOG_LP(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

}

