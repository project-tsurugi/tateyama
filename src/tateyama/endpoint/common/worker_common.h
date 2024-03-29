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
#pragma once

#include <future>
#include <thread>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <mutex>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/status/resource/bridge.h>
#include <tateyama/logging_helper.h>
#include <tateyama/session/resource/bridge.h>
#include <tateyama/session/variable_set.h>

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>
#include <tateyama/proto/diagnostics.pb.h>

#include "response.h"
#include "tateyama/endpoint/common/session_info_impl.h"

namespace tateyama::endpoint::common {

class worker_common {
public:
    enum class connection_type : std::uint32_t {
    /**
     * @brief undefined type.
     */
    undefined = 0U,

    /**
     * @brief IPC connection.
     */
    ipc,

    /**
     * @brief stream (TCP/IP) connection.
     */
    stream,
    };

    worker_common(connection_type con, std::size_t session_id, std::string_view conn_info, std::shared_ptr<tateyama::session::resource::bridge> session)
        : connection_type_(con),
          session_id_(session_id),
          session_info_(session_id, connection_label(con), conn_info),
          session_(std::move(session)),
          session_variable_set_(variable_declarations()),
          session_context_(std::make_shared<tateyama::session::resource::session_context_impl>(session_info_, session_variable_set_))
        {
            if (session_) {
                session_->register_session(session_context_);
            }
    }
    worker_common(connection_type con, std::size_t id, std::shared_ptr<tateyama::session::resource::bridge> session) : worker_common(con, id, "", std::move(session)) {
    }
    void invoke(std::function<void(void)> func) {
        task_ = std::packaged_task<void()>(std::move(func));
        future_ = task_.get_future();
        thread_ = std::thread(std::move(task_));
    }
    [[nodiscard]] auto wait_for() const {
        return future_.wait_for(std::chrono::seconds(0));
    }

protected:
    const connection_type connection_type_; // NOLINT
    const std::size_t session_id_;          // NOLINT
    session_info_impl session_info_;        // NOLINT

    // for ipc endpoint only
    std::string connection_info_{};         // NOLINT
    // for stream endpoint only
    std::size_t max_result_sets_{};         // NOLINT

    // for future
    std::packaged_task<void()> task_;       // NOLINT
    std::future<void> future_;              // NOLINT
    std::thread thread_{};                  // NOLINT

    // for session management
    const std::shared_ptr<tateyama::session::resource::bridge> session_;  // NOLINT

    bool handshake(tateyama::api::server::request* req, tateyama::api::server::response* res) {
        if (req->service_id() != tateyama::framework::service_id_endpoint_broker) {
            LOG(INFO) << "request received is not handshake";
            std::stringstream ss;
            ss << "handshake operation is required to establish sessions (service ID=" << req->service_id() << ")." << std::endl;
            ss << "see https://github.com/project-tsurugi/tsurugidb/blob/master/docs/upgrade-guide.md#handshake-required";
            notify_client(res, tateyama::proto::diagnostics::Code::ILLEGAL_STATE, ss.str());
            return false;
        }
        auto data = req->payload();
        tateyama::proto::endpoint::request::Request rq{};
        if(! rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string error_message{"request parse error"};
            LOG(INFO) << error_message;
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }
        if(rq.command_case() != tateyama::proto::endpoint::request::Request::kHandshake) {
            std::stringstream ss;
            ss << "bad request (handshake in endpoint): " << rq.command_case();
            LOG(INFO) << ss.str();
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
            return false;
        }
        auto ci = rq.handshake().client_information();
        session_info_.label(ci.connection_label());
        session_info_.application_name(ci.application_name());
        // FIXME handle userName when a credential specification is fixed.

        auto wi = rq.handshake().wire_information();
        switch (connection_type_) {
        case connection_type::ipc:
            if(wi.wire_information_case() != tateyama::proto::endpoint::request::WireInformation::kIpcInformation) {
                std::stringstream ss;
                ss << "bad wire information (handshake in endpoint): " << wi.wire_information_case();
                LOG(INFO) << ss.str();
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
                return false;
            }
            session_info_.connection_information(wi.ipc_information().connection_information());
            break;
        case connection_type::stream:
            if(wi.wire_information_case() != tateyama::proto::endpoint::request::WireInformation::kStreamInformation) {
                std::stringstream ss;
                ss << "bad wire information (handshake in endpoint): " << wi.wire_information_case();
                LOG(INFO) << ss.str();
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
                return false;
            }
            max_result_sets_ = wi.stream_information().maximum_concurrent_result_sets();  // for Stream
            break;
        default:  // shouldn't happen
            std::stringstream ss;
            ss << "illegal connection type: " << static_cast<std::uint32_t>(connection_type_);
            LOG(INFO) << ss.str();
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
            return false;
        }
        VLOG_LP(log_trace) << session_info_;  //NOLINT

        tateyama::proto::endpoint::response::Handshake rp{};
        auto rs = rp.mutable_success();
        rs->set_session_id(session_id_);
        auto body = rp.SerializeAsString();
        res->body(body);
        rp.clear_success();
        return true;
    }

    void notify_client(tateyama::api::server::response* res,
                       tateyama::proto::diagnostics::Code code,
                       const std::string& message) {
        tateyama::proto::diagnostics::Record record{};
        record.set_code(code);
        record.set_message(message);
        res->error(record);
        record.release_message();
    }

    bool endpoint_service(const std::shared_ptr<tateyama::api::server::request>& req,
                          [[maybe_unused]] const std::shared_ptr<tateyama::api::server::response>& res,
                          std::size_t slot) {
        auto data = req->payload();
        tateyama::proto::endpoint::request::Request rq{};
        if(! rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string error_message{"request parse error"};
            LOG(INFO) << error_message;
            notify_client(res.get(), tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }
        if(rq.command_case() != tateyama::proto::endpoint::request::Request::kCancel) {
            std::stringstream ss;
            ss << "bad request (cancel in endpoint): " << rq.command_case();
            LOG(INFO) << ss.str();
            notify_client(res.get(), tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(mtx_responses_);
            if (auto itr = responses_.find(slot); itr != responses_.end()) {
                if (auto ptr = itr->second.lock(); ptr) {
                    ptr->set_cancel();
                }
            }
        }
        return true;
    }

    void register_response(std::size_t slot, const std::shared_ptr<tateyama::endpoint::common::response>& response) noexcept {
        std::lock_guard<std::mutex> lock(mtx_responses_);
        if (auto itr = responses_.find(slot); itr != responses_.end()) {
            responses_.erase(itr);
        }
        responses_.emplace(slot, response);
    }
    void remove_response(std::size_t slot) noexcept {
        std::lock_guard<std::mutex> lock(mtx_responses_);
        responses_.erase(slot);
    }

private:
    tateyama::session::session_variable_set session_variable_set_;
    const std::shared_ptr<tateyama::session::resource::session_context_impl> session_context_;
    std::map<std::size_t, std::weak_ptr<tateyama::endpoint::common::response>> responses_{};
    std::mutex mtx_responses_{};

    std::string_view connection_label(connection_type con) {
        switch (con) {
        case connection_type::ipc:
            return "IPC";
        case connection_type::stream:
            return "TCP/IP";
        default:
            return "";
        }
    }

    [[nodiscard]] std::vector<std::tuple<std::string, tateyama::session::session_variable_set::variable_type, tateyama::session::session_variable_set::value_type>> variable_declarations() const noexcept {
        return {
            { "example_integer", tateyama::session::session_variable_type::signed_integer, static_cast<std::int64_t>(0) }
        };
    }

};

}  // tateyama::endpoint::common
