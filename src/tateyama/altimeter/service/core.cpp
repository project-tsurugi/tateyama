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
#include <altimeter/logger.h>
#include <altimeter/event/event_logger.h>

#include <tateyama/altimeter/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/altimeter/request.pb.h>
#include <tateyama/proto/altimeter/response.pb.h>
#include <tateyama/proto/diagnostics.pb.h>

namespace tateyama::altimeter::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

class altimeter_helper_direct : public altimeter_helper {
public:
    void set_enabled(std::string_view t, const bool& v) override {
        ::altimeter::logger::set_enabled(t, v);
    }
    void set_level(std::string_view t, std::uint64_t v) override {
        ::altimeter::logger::set_level(t, static_cast<std::int32_t>(v));
    }
    void set_stmt_duration_threshold(std::uint64_t v) override {
        ::altimeter::event::event_logger::set_stmt_duration_threshold(static_cast<std::int64_t>(v));
    }
    void rotate_all(std::string_view t) override {
        ::altimeter::logger::rotate_all(t);
    }
};

bool tateyama::altimeter::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    tateyama::proto::altimeter::request::Request rq{};

    auto data = req->payload();
    if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        LOG(INFO) << "request parse error";
        return true;  // It indicates a possible malfunction in the communication system, but let's continue the operation.
    }

    res->session_id(req->session_id());
    switch(rq.command_case()) {
    case tateyama::proto::altimeter::request::Request::kConfigure:
    {
        const auto& configure = rq.configure();
        if (configure.has_event_log()) {
            const auto& log_settings = configure.event_log();
            if (log_settings.enabled_opt_case() == tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::kEnabled) {
                helper_->set_enabled("event", log_settings.enabled());
            }
            if (log_settings.level_opt_case() == tateyama::proto::altimeter::request::LogSettings::LevelOptCase::kLevel) {
                auto v = log_settings.level();
                helper_->set_level("event", v);
            }
            if (log_settings.statement_duration_opt_case() == tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::kStatementDuration) {
                auto v = log_settings.statement_duration();
                helper_->set_stmt_duration_threshold(v);
            }
        }
        if (configure.has_audit_log()) {
            const auto& log_settings = configure.audit_log();
            if (log_settings.enabled_opt_case() == tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::kEnabled) {
                helper_->set_enabled("audit", log_settings.enabled());
            }
            if (log_settings.level_opt_case() == tateyama::proto::altimeter::request::LogSettings::LevelOptCase::kLevel) {
                auto v = log_settings.level();
                helper_->set_level("audit", v);
            }
        }
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        res->body(rs.SerializeAsString());
        rs.clear_success();
        break;
    }

    case tateyama::proto::altimeter::request::Request::kLogRotate:
    {
        const auto& log_lotate = rq.log_rotate();
        switch(log_lotate.category()) {
        case tateyama::proto::altimeter::common::LogCategory::EVENT:
            helper_->rotate_all("event");
            break;
        case tateyama::proto::altimeter::common::LogCategory::AUDIT:
            helper_->rotate_all("audit");
            break;
        default:
            send_error<tateyama::proto::altimeter::response::LogRotate>(res, tateyama::proto::altimeter::response::ErrorKind::UNKNOWN, "log type for LogRotate is invalid");
            return true;  // Error notification is treated as normal termination.
        }
        tateyama::proto::altimeter::response::LogRotate rs{};
        (void) rs.mutable_success();
        res->body(rs.SerializeAsString());
        rs.clear_success();
        break;
    }

    case tateyama::proto::altimeter::request::Request::COMMAND_NOT_SET:
    {
        tateyama::proto::diagnostics::Record record{};
        record.set_code(tateyama::proto::diagnostics::Code::INVALID_REQUEST);
        record.set_message("request for altimeter is invalid");
        res->error(record);
        break;
    }

    }
    return true;
}

core::core() : helper_(std::dynamic_pointer_cast<altimeter_helper>(std::make_shared<altimeter_helper_direct>())) {
}

}
