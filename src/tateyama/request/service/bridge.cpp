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

#include <chrono>
#include <map>
#include <tuple>

#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/proto/request/request.pb.h>
#include <tateyama/proto/request/response.pb.h>
#include <tateyama/proto/diagnostics.pb.h>

#include "tateyama/endpoint/common/listener_common.h"
#include "bridge.h"

namespace tateyama::request::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment&) {
    return true;
}

bool bridge::start(environment&) {
    return true;
}

bool bridge::shutdown(environment&) {
    return true;
}

bool bridge::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    tateyama::proto::request::request::Request rq{};
    auto data = req->payload();
    if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        LOG(ERROR) << "request parse error";
        return false;
    }

    res->session_id(req->session_id());
    switch(rq.command_case()) {
    case tateyama::proto::request::request::Request::CommandCase::kListRequest:
    {
        tateyama::proto::request::response::ListRequest rs{};
        auto &lr = rq.list_request();
        std::optional<std::uint32_t> top_opt{};
        std::optional<std::uint32_t> service_id_opt{};
        if (lr.top_opt_case() == tateyama::proto::request::request::ListRequest::TopOptCase::kTop) {
            top_opt = lr.top();
        }
        if (lr.service_filter_case() == tateyama::proto::request::request::ListRequest::ServiceFilterCase::kServiceId) {
            service_id_opt = lr.service_id();
        }
        std::multimap<std::size_t, std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>> results{};
        foreach_endpoint([&service_id_opt, &results](const std::shared_ptr<tateyama::api::server::request>& request, std::chrono::system_clock::time_point tp) {
            bool is_target{};
            if (request->service_id() != tateyama::framework::service_id_request) {
                if (service_id_opt) {
                    is_target = (service_id_opt.value() == request->service_id());
                } else {
                    is_target = true;
                }
                if (is_target) {
                    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
                    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) -
                        std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);
                    auto started_time = secs.time_since_epoch().count() * 1000 + (ns.count() + 500000) / 1000000;
                    results.emplace(started_time, std::make_tuple(request->session_id(), request->local_id(), request->service_id(), request->payload().length()));
                }
            }
        });

        auto* success = rs.mutable_success();
        std::uint32_t count = 0;
        for (const auto& result : results) {
            auto* ri = success->add_requests();
            ri->set_session_id(std::get<0>(result.second));
            ri->set_request_id(std::get<1>(result.second));
            ri->set_service_id(std::get<2>(result.second));
            ri->set_payload_size(std::get<3>(result.second));
            ri->set_started_time(result.first);
            count++;
            if (top_opt) {
                if (top_opt.value() <= count) {
                    break;
                }
            }
        }
        res->body(rs.SerializeAsString());
        return true;
    }
    case tateyama::proto::request::request::Request::CommandCase::kGetPayload:
    {
        tateyama::proto::request::response::GetPayload rs{};
        auto& gp = rq.get_payload();
        std::size_t session_id = gp.session_id();
        std::size_t request_id = gp.request_id();
        foreach_endpoint([session_id, request_id, &rs](const std::shared_ptr<tateyama::api::server::request>& request, std::chrono::system_clock::time_point) {
            if (session_id == request->session_id() && request_id == request->local_id()) {
                rs.mutable_success()->set_data(std::string(request->payload()));
            }
        });
        if (!rs.has_success()) {
            auto* error = rs.mutable_error();
            error->set_code(tateyama::proto::request::diagnostics::Code::REQUEST_MISSING);
        }
        res->body(rs.SerializeAsString());
        return true;
    }
    case tateyama::proto::request::request::Request::CommandCase::COMMAND_NOT_SET:
    default:
    {
        tateyama::proto::diagnostics::Record record{};
        record.set_code(tateyama::proto::diagnostics::Code::INVALID_REQUEST);
        res->error(record);
        break;
    }
    }
    return false;
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

std::string_view bridge::label() const noexcept {
    return component_label;
}

void bridge::register_endpoint_listener(std::shared_ptr<tateyama::endpoint::common::listener_common> listener) {
    listeners_.emplace_back(std::move(listener));
}

}
