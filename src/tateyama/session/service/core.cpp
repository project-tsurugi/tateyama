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
#include <thread>

#include <tateyama/session/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>
#include <tateyama/proto/session/diagnostic.pb.h>

#include "tateyama/session/resource/context_impl.h"
#include "tateyama/endpoint/common/worker_common.h"

namespace tateyama::session::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

bool tateyama::session::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    bool return_code =true;
    tateyama::proto::session::request::Request rq{};

    auto data = req->payload();
    if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        LOG(ERROR) << "request parse error";
        return false;
    }

    res->session_id(req->session_id());
    switch(rq.command_case()) {
    case tateyama::proto::session::request::Request::kSessionGet:
    {
        tateyama::proto::session::response::SessionGet rs{};
        auto& cmd = rq.session_get();
        auto rv = resource_->get(cmd.session_specifier(), rs.mutable_success());
        if (!rv) {
            res->body(rs.SerializeAsString());
        } else {
            send_error<tateyama::proto::session::response::SessionGet>(res, rv.value());
            return_code = false;
        }
        rs.clear_success();
        break;
    }

    case tateyama::proto::session::request::Request::kSessionList:
    {
        tateyama::proto::session::response::SessionList rs{};
        auto rv = resource_->list(rs.mutable_success());
        if (!rv) {
            res->body(rs.SerializeAsString());
        } else {
            send_error<tateyama::proto::session::response::SessionList>(res, rv.value());
            return_code = false;
        }
        rs.clear_success();
        break;
    }

    case tateyama::proto::session::request::Request::kSessionShutdown:
    {
        auto& cmd = rq.session_shutdown();
        tateyama::session::shutdown_request_type type{};
        switch (cmd.request_type()) {
        case tateyama::proto::session::request::SessionShutdownType::SESSION_SHUTDOWN_TYPE_NOT_SET:
        case tateyama::proto::session::request::SessionShutdownType::GRACEFUL:
            type = tateyama::session::shutdown_request_type::graceful;
            break;
        case tateyama::proto::session::request::SessionShutdownType::FORCEFUL:
            type = tateyama::session::shutdown_request_type::forceful;
            break;
        default:
            send_error<tateyama::proto::session::response::SessionShutdown>(res);
            return false;
        }
        std::shared_ptr<tateyama::session::session_context> session_context{};
        auto rv = resource_->session_shutdown(cmd.session_specifier(), type, session_context);
        if (!rv) {
            std::thread th([res, session_context]{
                while (true) {
                    auto* session_context_impl = reinterpret_cast<tateyama::session::resource::session_context_impl*>(session_context.get());  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    auto worker = session_context_impl->get_session_worker();
                    if (!worker) {
                        break;
                    }
                    if (worker->terminated()) {
                        break;
                    }
                }
                tateyama::proto::session::response::SessionShutdown rs{};
                rs.mutable_success();
                res->body(rs.SerializeAsString());
                rs.clear_success();
            });
            th.detach();
        } else {
            send_error<tateyama::proto::session::response::SessionShutdown>(res, rv.value());
            return false;
        }
        break;
    }

    case tateyama::proto::session::request::Request::kSessionSetVariable:
    {
        auto& cmd = rq.session_set_variable();
        auto rv = resource_->set_valiable(cmd.session_specifier(), cmd.name(), cmd.value());
        if (!rv) {
            tateyama::proto::session::response::SessionSetVariable rs{};
            rs.mutable_success();
            res->body(rs.SerializeAsString());
            rs.clear_success();
        } else {
            send_error<tateyama::proto::session::response::SessionSetVariable>(res, rv.value());
            return false;
        }
        break;
    }

    case tateyama::proto::session::request::Request::kSessionGetVariable:
    {
        tateyama::proto::session::response::SessionGetVariable rs{};
        auto& cmd = rq.session_get_variable();
        auto rv = resource_->get_valiable(cmd.session_specifier(), cmd.name(), rs.mutable_success());
        if (!rv) {
            res->body(rs.SerializeAsString());
        } else {
            send_error<tateyama::proto::session::response::SessionGetVariable>(res, rv.value());
            return_code = false;
        }
        rs.clear_success();
        break;
    }

    case tateyama::proto::session::request::Request::COMMAND_NOT_SET:
        return false;
    }

    return return_code;
}

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{}

bool core::start(tateyama::session::resource::bridge* resource) {
    resource_ = resource;
    return true;
}

bool core::shutdown(bool force) {
    //TODO implement
    (void) force;
    return true;
}

}
