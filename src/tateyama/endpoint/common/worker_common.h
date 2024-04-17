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
#pragma once

#include <future>
#include <thread>
#include <memory>
#include <functional>
#include <map>
#include <utility>
#include <vector>
#include <mutex>

#include <tateyama/status.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/status/resource/bridge.h>
#include <tateyama/logging_helper.h>
#include <tateyama/session/resource/bridge.h>
#include <tateyama/session/variable_set.h>
#include <tateyama/api/server/session_store.h>

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>
#include <tateyama/proto/diagnostics.pb.h>

#include "request.h"
#include "response.h"
#include "session_info_impl.h"

namespace tateyama::endpoint::common {

class worker_common {
protected:
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

public:
    worker_common(connection_type con, std::size_t session_id, std::string_view conn_info, std::shared_ptr<tateyama::session::resource::bridge> session)
        : connection_type_(con),
          session_id_(session_id),
          session_info_(session_id, connection_label(con), conn_info),
          session_(std::move(session)),
          session_variable_set_(variable_declarations()),
          session_context_(std::make_shared<tateyama::session::resource::session_context_impl>(session_info_, session_variable_set_)) {
        if (session_) {
            session_->register_session(session_context_);
        }
    }
    worker_common(connection_type con, std::size_t id, std::shared_ptr<tateyama::session::resource::bridge> session) : worker_common(con, id, "", std::move(session)) {
    }
    virtual ~worker_common() {
        if(thread_.joinable()) thread_.join();
    }

    /**
     * @brief Copy and move constructers are delete.
     */
    worker_common(worker_common const&) = delete;
    worker_common(worker_common&&) = delete;
    worker_common& operator = (worker_common const&) = delete;
    worker_common& operator = (worker_common&&) = delete;

    void invoke(std::function<void(void)> func) {
        task_ = std::packaged_task<void()>(std::move(func));
        future_ = task_.get_future();
        thread_ = std::thread(std::move(task_));
    }

    [[nodiscard]] auto wait_for() const {
        return future_.wait_for(std::chrono::seconds(0));
    }

    [[nodiscard]] bool terminated() const {
        return wait_for() == std::future_status::ready;
    }

    [[nodiscard]] bool is_quiet() {
        return !has_incomplete_response() && !has_incomplete_resultset();
    }

    /**
     * @brief Register this in the session_context.
     * @param me the shared_ptr of this object
     */
    void register_worker_in_context(const std::shared_ptr<worker_common>& me) {
        if (me.get() != this) {
            throw std::runtime_error("invalid argument");
        }
        session_context_->set_worker(me);
    }

    /**
     * @brief dispose the session_elements in the session_store.
     */
    void dispose_session_store() {
        session_store_.dispose();
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

    // session store
    tateyama::api::server::session_store session_store_{};  // NOLINT

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

    static void notify_client(tateyama::api::server::response* res,
                       tateyama::proto::diagnostics::Code code,
                       const std::string& message) {
        tateyama::proto::diagnostics::Record record{};
        record.set_code(code);
        record.set_message(message);
        res->error(record);
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

        switch (rq.command_case()) {

        case tateyama::proto::endpoint::request::Request::kCancel:
        {
            VLOG_LP(log_trace) << "received cancel request, slot = " << slot;  //NOLINT
            {
                std::lock_guard<std::mutex> lock(mtx_reqreses_);
                if (auto itr = reqreses_.find(slot); itr != reqreses_.end()) {
                    itr->second.second->cancel();
                }
            }
            return true;
        }

        default: // error
        {
            std::stringstream ss;
            ss << "bad request (cancel in endpoint): " << rq.command_case();
            LOG(INFO) << ss.str();
            notify_client(res.get(), tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
        }
        return false;
        }
    }

    void register_reqres(std::size_t slot, const std::shared_ptr<tateyama::api::server::request>& request, const std::shared_ptr<tateyama::endpoint::common::response>& response) noexcept {
        std::lock_guard<std::mutex> lock(mtx_reqreses_);
        if (auto itr = reqreses_.find(slot); itr != reqreses_.end()) {
            reqreses_.erase(itr);
        }
        reqreses_.emplace(slot, std::pair<std::shared_ptr<tateyama::api::server::request>, std::shared_ptr<tateyama::endpoint::common::response>>(request, response));
    }

    void remove_reqres(std::size_t slot) noexcept {
        std::lock_guard<std::mutex> lock(mtx_reqreses_);
        if (auto&& itr = reqreses_.find(slot); itr != reqreses_.end()) {
            reqreses_.erase(itr);
        }
    }

    bool is_shuttingdown() {
        auto sr = session_context_->shutdown_request();
        if ((sr == tateyama::session::shutdown_request_type::forceful) && !cancel_requested_to_all_responses_) {
            foreach_reqreses([](tateyama::endpoint::common::response& r){ r.cancel(); });
            cancel_requested_to_all_responses_ = true;
        }
        return (sr == tateyama::session::shutdown_request_type::graceful) ||
            (sr == tateyama::session::shutdown_request_type::forceful);
    }

    void care_reqreses() {
        std::vector<std::shared_ptr<tateyama::endpoint::common::response>> targets{};
        {
            std::lock_guard<std::mutex> lock(mtx_reqreses_);
            for (auto itr{reqreses_.begin()}, end{reqreses_.end()}; itr != end; ) {
                auto&& pair = itr->second;
                auto && res = itr->second.second;
                if ((pair.first.use_count() == 1) && (res.use_count() == 1)) {
                    if (!res->is_completed()) {
                        targets.emplace_back(res);
                    }
                    itr = reqreses_.erase(itr);
                } else {
                    ++itr;
                }
            }
        }
        for (auto &&e: targets) {
            notify_client(e.get(), tateyama::proto::diagnostics::Code::UNKNOWN, "request dissipated");
        }
    }

    bool is_completed() {
        std::lock_guard<std::mutex> lock(mtx_reqreses_);
        return reqreses_.empty();
    }

    void foreach_reqreses(const std::function<void(tateyama::endpoint::common::response&)>& func) {
        std::vector<std::shared_ptr<tateyama::endpoint::common::response>> targets{};
        {
            std::lock_guard<std::mutex> lock(mtx_reqreses_);
            for (auto itr{reqreses_.begin()}, end{reqreses_.end()}; itr != end; ) {
                auto& ptr = itr->second.second;
                if (ptr.use_count() > 1) {
                    targets.emplace_back(ptr);
                    ++itr;
                } else {
                    if (itr->second.first.use_count() == 1) {
                        itr = reqreses_.erase(itr);
                    } else {
                        ++itr;
                    }
                }
            }
        }
        for (auto &&e: targets) {
            func(*e);
        }
    }

    bool request_shutdown(tateyama::session::shutdown_request_type type) noexcept {
        if (type == session::shutdown_request_type::forceful) {
            if (!cancel_requested_to_all_responses_) {
                foreach_reqreses([](tateyama::endpoint::common::response& r){ r.cancel(); });
                cancel_requested_to_all_responses_ = true;
            }
        }
        return session_context_->shutdown_request(type);
    }

    [[nodiscard]] bool has_incomplete_response() {
        bool rv{false};
        foreach_reqreses([&rv](tateyama::endpoint::common::response& r){if(!r.is_completed()){rv = true;}});
        return rv;
    }

    virtual bool has_incomplete_resultset() = 0;

private:
    tateyama::session::session_variable_set session_variable_set_;
    const std::shared_ptr<tateyama::session::resource::session_context_impl> session_context_;
    std::map<std::size_t, std::pair<std::shared_ptr<tateyama::api::server::request>, std::shared_ptr<tateyama::endpoint::common::response>>> reqreses_{};
    std::mutex mtx_reqreses_{};
    bool cancel_requested_to_all_responses_{};

    std::string_view connection_label(connection_type con) {
        switch (con) {
        case connection_type::ipc:
            return "ipc";
        case connection_type::stream:
            return "tcp";
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
