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
#include <tateyama/session/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>
#include <tateyama/proto/session/diagnostic.pb.h>

namespace tateyama::session::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

bool tateyama::session::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    tateyama::proto::session::request::Request rq{};

    auto data = req->payload();
    if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        LOG(ERROR) << "request parse error";
        return false;
    }

    switch(rq.command_case()) {
    case tateyama::proto::session::request::Request::kSessionGet:
    {
        auto& cmd = rq.session_get();
        auto rv = resource_->get(cmd.session_specifier());
        if (!rv) {
            tateyama::proto::session::response::SessionGet rs{};
            rs.mutable_success();
            res->body(rs.SerializeAsString());
            rs.clear_success();
        } else {
            send_error<tateyama::proto::session::response::SessionGet>(res, rv.value());
        }
        break;
    }

    case tateyama::proto::session::request::Request::kSessionList:
    {
        auto rv = resource_->list();
        if (!rv) {
            tateyama::proto::session::response::SessionList rs{};
            rs.mutable_success();
            res->body(rs.SerializeAsString());
            rs.clear_success();
        } else {
            send_error<tateyama::proto::session::response::SessionGet>(res, rv.value());
        }
        break;
    }

    case tateyama::proto::session::request::Request::kSessionShutdown:
    {
        auto& cmd = rq.session_shutdown();
        tateyama::session::resource::shutdown_request_type type{};
        switch (cmd.request_type()) {
        case tateyama::proto::session::request::SessionShutdownType::SESSION_SHUTDOWN_TYPE_NOT_SET:
        case tateyama::proto::session::request::SessionShutdownType::GRACEFUL:
            type = tateyama::session::resource::shutdown_request_type::graceful;
            break;
        case tateyama::proto::session::request::SessionShutdownType::FORCEFUL:
            type = tateyama::session::resource::shutdown_request_type::forceful;
            break;
        default:
            send_error<tateyama::proto::session::response::SessionSetVariable>(res);
            return false;
        }
        auto rv = resource_->shutdown(cmd.session_specifier(), type);
        if (!rv) {
            tateyama::proto::session::response::SessionSetVariable rs{};
            rs.mutable_success();
            res->body(rs.SerializeAsString());
            rs.clear_success();
        } else {
            send_error<tateyama::proto::session::response::SessionSetVariable>(res, rv.value());
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
        auto& cmd = rq.session_get_variable();
        auto rv = resource_->get_valiable(cmd.session_specifier(), cmd.name());
        if (!rv) {
            tateyama::proto::session::response::SessionGetVariable rs{};
            rs.mutable_success();
            res->body(rs.SerializeAsString());
            rs.clear_success();
        } else {
            send_error<tateyama::proto::session::response::SessionGetVariable>(res, rv.value());
            return false;
        }
        break;
    }

    case tateyama::proto::session::request::Request::COMMAND_NOT_SET:
        return false;
    }

    return true;
}

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{}

bool core::start(tateyama::session::resource::bridge* resource) {
    resource_ = resource;
    //TODO implement
    return true;
}

bool core::shutdown(bool force) {
    //TODO implement
    (void) force;
    return true;
}

}
