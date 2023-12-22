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

#include <glog/logging.h>

#include <takatori/util/exception.h>
#include <tateyama/debug/service.h>
#include <tateyama/proto/debug/request.pb.h>
#include <tateyama/proto/debug/response.pb.h>

using takatori::util::throw_exception;

namespace tateyama::debug {

using tateyama::api::server::request;
using tateyama::api::server::response;
namespace framework = tateyama::framework;

constexpr static std::string_view log_location_prefix = "/:tateyama:debug:logging ";

framework::component::id_type service::id() const noexcept {
    return tag;
}

bool service::setup(framework::environment&) {
    return true;
}

bool service::start(framework::environment&) {
    return true;
}

bool service::shutdown(framework::environment&) {
    return true;
}

service::~service() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

static void reply(google::protobuf::Message &message,
                  std::shared_ptr<tateyama::api::server::response> &res) {
    std::string s { };
    if (!message.SerializeToString(&s)) {
        throw_exception(std::logic_error{"SerializeToOstream failed"});
    }
    res->body(s);
}

static void reply_error(std::string_view error_message,
                  std::shared_ptr<tateyama::api::server::response> &res) {
    tateyama::proto::debug::response::UnknownError error { };
    tateyama::proto::debug::response::Logging logging { };
    error.set_message(std::string(error_message));
    logging.set_allocated_unknown_error(&error);
    reply(logging, res);
    logging.release_unknown_error();
}

static void success_logging(std::shared_ptr<tateyama::api::server::response> &res) {
    tateyama::proto::debug::response::Logging logging { };
    tateyama::proto::debug::response::Void v { };
    logging.set_allocated_success(&v);
    reply(logging, res);
    logging.release_success();
}

static void command_logging(tateyama::proto::debug::request::Request &proto_req,
                          std::shared_ptr<response> &res) {
    const auto &logging = proto_req.logging();
    const auto &message = logging.message();
    switch (logging.level()) {
        case tateyama::proto::debug::request::Logging_Level::Logging_Level_NOT_SPECIFIED:
        case tateyama::proto::debug::request::Logging_Level::Logging_Level_INFO:
            LOG(INFO) << log_location_prefix << message;
            break;
        case tateyama::proto::debug::request::Logging_Level::Logging_Level_WARN:
            LOG(WARNING) << log_location_prefix << message;
            break;
        case tateyama::proto::debug::request::Logging_Level::Logging_Level_ERROR:
            LOG(ERROR) << log_location_prefix << message;
            break;
        default:
            throw_exception(std::logic_error{"unknown Logging_Level value"});
    }
    success_logging(res);
}

bool service::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    tateyama::proto::debug::request::Request proto_req { };
    res->session_id(req->session_id());
    auto s = req->payload();
    if (!proto_req.ParseFromArray(s.data(), static_cast<int>(s.size()))) {
        reply_error("parse error with request body", res);
        return false;
    }
    switch (proto_req.command_case()) {
        case tateyama::proto::debug::request::Request::kLogging:
            command_logging(proto_req, res);
            break;
        default:
            reply_error("unknown command_case", res);
            break;
    }
    return true;
}

std::string_view service::label() const noexcept {
    return component_label;
}

} // namespace tateyama::debug
