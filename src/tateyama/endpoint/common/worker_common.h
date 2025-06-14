/*
 * Copyright 2018-2025 Project Tsurugi.
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
#include <chrono>

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
#include <tateyama/proto/core/request.pb.h>
#include <tateyama/proto/core/response.pb.h>
#include <tateyama/proto/diagnostics.pb.h>

#include "request.h"
#include "response.h"
#include "listener_common.h"
#include "worker_configuration.h"

namespace tateyama::endpoint::common {

class worker_common {
public:
    worker_common(const configuration& config, std::size_t session_id, std::string_view conn_info)
        : connection_type_(config.con_),
          session_(config.session_),
          resources_(config, session_id, conn_info),
          session_context_(std::make_shared<tateyama::session::resource::session_context_impl>(resources_.session_info(), resources_.session_variable_set())),
          enable_timeout_(config.enable_timeout_),
          refresh_timeout_(config.refresh_timeout_),
          max_refresh_timeout_(config.max_refresh_timeout_),
          auth_(config.auth_.get()) {
        if (session_) {
            session_->register_session(session_context_);
        }
        update_expiration_time(true);
    }
    virtual ~worker_common() {
        if(thread_.joinable()) thread_.join();
        auto& session_info = resources_.session_info();
        if (auto name_opt = session_info.username(); name_opt) {
            LOG_LP(INFO) << "session (" << resources_.session_id() << ") of the authenticated user (" << name_opt.value() << ") end";
        }
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

    [[nodiscard]] bool is_terminated() const {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    [[nodiscard]] bool is_quiet() {
        if (has_incomplete_response()) {  // should check before has_incomplete_resultset()
            return false;
        }
        if (has_incomplete_resultset()) {
            return false;
        }
        return is_terminated();
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
        if (!dispose_done_) {
            resources_.session_store().dispose();
            dispose_done_ = true;
        }
    }

    /**
     * @brief print diagnostics
     */
    void print_diagnostic(std::ostream& os, bool live) {
        os << "    session id = " << resources_.session_id() << std::endl;
        os << "      processing requests" << std::endl;
        std::lock_guard<std::mutex> lock(mtx_reqreses_);
        for (auto itr{reqreses_.begin()}, end{reqreses_.end()}; itr != end; ++itr) {
            os << "       slot " << std::dec << itr->first << std::endl;
            os << "         service id = " << itr->second.first->service_id() << std::endl;
            os << "         local id = " << itr->second.first->local_id() << std::endl;
            os << "         request message = ";
            dump_message(os, itr->second.first->payload());
            os << std::endl;
            os << "         data channel status = '" << itr->second.second->state_label() << "'" << std::endl;
        }
        if (!live) {
            os << "       has_incomplete_response " << std::boolalpha << has_incomplete_response() << std::endl;
            os << "       has_incomplete_resultset " << std::boolalpha << has_incomplete_resultset() << std::endl;
            os << "       is_terminated " << std::boolalpha << is_terminated() << std::endl;
        }
    }

    /**
     * @brief apply callback function to ongoing request
     * @param func the callback function
     */
    void foreach_request(const listener_common::callback& func) {
        std::vector<std::shared_ptr<tateyama::endpoint::common::request>> targets{};
        {
            std::lock_guard<std::mutex> lock(mtx_reqreses_);
            for (auto itr{reqreses_.begin()}, end{reqreses_.end()}; itr != end; itr++) {
                auto& ptr = itr->second.first;
                if (ptr.use_count() > 1) {
                    targets.emplace_back(ptr);
                }
            }
        }
        for (auto&& e: targets) {
            func(std::dynamic_pointer_cast<tateyama::api::server::request>(e), e->start_at());
        }
    }
    /**
     * @brief returns session id
     */
    [[nodiscard]] inline std::size_t session_id() const noexcept { return resources_.session_id(); }

protected:
    const connection_type connection_type_; // NOLINT

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

    // local_id
    std::size_t local_id_{};  // NOLINT

    bool handshake(tateyama::api::server::request* req, tateyama::api::server::response* res) {
        if (req->service_id() != tateyama::framework::service_id_endpoint_broker) {
            LOG_LP(INFO) << "request received is not handshake";
            std::stringstream ss;
            ss << "handshake operation is required to establish sessions (service ID=" << req->service_id() << ")." << std::endl;
            ss << "see https://github.com/project-tsurugi/tsurugidb/blob/master/docs/upgrade-guide.md#handshake-required";
            notify_client(res, tateyama::proto::diagnostics::Code::ILLEGAL_STATE, ss.str());
            return false;
        }
        auto data = req->payload();
        tateyama::proto::endpoint::request::Request rq{};
        if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string error_message{"request parse error"};
            LOG_LP(INFO) << error_message;
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }
        switch (rq.command_case()) {
        case tateyama::proto::endpoint::request::Request::kEncryptionKey:
        {
            tateyama::proto::endpoint::response::EncryptionKey rp{};
            if (auth_) {
                if (auto key_opt = auth_->get_encryption_key(); key_opt) {
                    auto rs = rp.mutable_success();
                    rs->set_encryption_key(key_opt.value());
                    res->body(rp.SerializeAsString());
                    rp.clear_success();
                    throw psudo_exception_of_continue();
                }
                auto re = rp.mutable_error();
                re->set_code(tateyama::proto::diagnostics::Code::SYSTEM_ERROR);
                re->set_message("encryptionKey is not available");
                res->body(rp.SerializeAsString());
                rp.clear_error();
                return false;
            }
            auto re = rp.mutable_error();
            re->set_code(tateyama::proto::diagnostics::Code::UNSUPPORTED_OPERATION);
            re->set_message("authentication is off");
            res->body(rp.SerializeAsString());
            rp.clear_error();
            throw psudo_exception_of_continue();
        }
        case tateyama::proto::endpoint::request::Request::kHandshake:
            break;

        default:
            std::stringstream ss;
            ss << "bad request (handshake in endpoint): " << rq.command_case();
            LOG_LP(INFO) << ss.str();
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
            return false;
        }

        // case tateyama::proto::endpoint::request::Request::kHandshake:
        auto ci = rq.handshake().client_information();
        std::string connection_label = ci.connection_label();
        if (!connection_label.empty()) {
            if (connection_label.at(0) == ':') {
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, "invalid connection label");
                return false;
            }
        }
        auto& session_info = resources_.session_info();
        session_info.label(connection_label);
        session_info.application_name(ci.application_name());

        auto& credential = ci.credential();
        if (auth_) {
            switch (credential.credential_opt_case()) {
            case tateyama::proto::endpoint::request::Credential::CredentialOptCase::kEncryptedCredential:
            {
                if (auto username_opt = auth_->verify_encrypted(credential.encrypted_credential()); username_opt) {
                    session_info.user_name(username_opt.value());
                } else {
                    notify_client(res, tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR, "user or password is incorrect");
                    return false;
                }
            }
            break;
            case tateyama::proto::endpoint::request::Credential::CredentialOptCase::kRememberMeCredential:
            {
                if (auto username_opt = auth_->verify_token(credential.remember_me_credential()); username_opt) {
                    session_info.user_name(username_opt.value());
                } else {
                    notify_client(res, tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR, "token is incorrect");
                    return false;
                }
            }
            break;
            default: 
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, "no valid credential");
                return false;
            }
            LOG_LP(INFO) << "session (" << resources_.session_id() << ") of an authenticated user (" << session_info.user_name() << ") begin";
        }

        auto wi = rq.handshake().wire_information();
        switch (connection_type_) {
        case connection_type::ipc:
            if(wi.wire_information_case() != tateyama::proto::endpoint::request::WireInformation::kIpcInformation) {
                std::stringstream ss;
                ss << "bad wire information (handshake in endpoint): " << wi.wire_information_case();
                LOG_LP(INFO) << ss.str();
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
                return false;
            }
            session_info.connection_information(wi.ipc_information().connection_information());
            break;
        case connection_type::stream:
            if(wi.wire_information_case() != tateyama::proto::endpoint::request::WireInformation::kStreamInformation) {
                std::stringstream ss;
                ss << "bad wire information (handshake in endpoint): " << wi.wire_information_case();
                LOG_LP(INFO) << ss.str();
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
                return false;
            }
            max_result_sets_ = wi.stream_information().maximum_concurrent_result_sets();  // for Stream
            break;
        default:  // shouldn't happen
            std::stringstream ss;
            ss << "illegal connection type: " << static_cast<std::uint32_t>(connection_type_);
            LOG_LP(INFO) << ss.str();
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
            return false;
        }
        VLOG_LP(log_trace) << session_info;  //NOLINT

        tateyama::proto::endpoint::response::Handshake rp{};
        auto rs = rp.mutable_success();
        rs->set_session_id(resources_.session_id());
        auto body = rp.SerializeAsString();
        res->body(body);
        rp.clear_success();
        return true;
    }

    class psudo_exception_of_continue : public std::exception {
        // no special member
    };

    static void notify_client(tateyama::api::server::response* res,
                       tateyama::proto::diagnostics::Code code,
                       const std::string& message) {
        tateyama::proto::diagnostics::Record record{};
        record.set_code(code);
        record.set_message(message);
        res->error(record);
    }

    bool endpoint_service(const std::shared_ptr<tateyama::api::server::request>& req,
                          const std::shared_ptr<tateyama::api::server::response>& res,
                          std::size_t slot) {
        auto data = req->payload();
        tateyama::proto::endpoint::request::Request rq{};
        if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string error_message{"request parse error"};
            LOG_LP(INFO) << error_message;
            notify_client(res.get(), tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }

        switch (rq.command_case()) {

        case tateyama::proto::endpoint::request::Request::kCancel:
        {
            VLOG_LP(log_trace) << "received cancel request, slot = " << slot;  //NOLINT
            if (auto sp = std::dynamic_pointer_cast<tateyama::endpoint::common::response>(res); sp) {
                // Do not respond to the cancellation request, but respond to the request to be cancelled.
                sp->set_completed();
            }
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
            ss << "bad request for endpoint: " << rq.command_case();
            LOG_LP(INFO) << ss.str();
            notify_client(res.get(), tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
        }
        return false;
        }
    }

    bool routing_service_chain(const std::shared_ptr<tateyama::api::server::request>& req,
                               const std::shared_ptr<tateyama::api::server::response>& res,
                               std::size_t index) {
        auto data = req->payload();
        tateyama::proto::core::request::Request rq{};
        if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string error_message{"request parse error"};
            LOG_LP(INFO) << error_message;
            notify_client(res.get(), tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }

        switch (rq.command_case()) {

        case tateyama::proto::core::request::Request::kShutdown:
        {
            VLOG_LP(log_trace) << "received shutdown request";  //NOLINT

            tateyama::session::shutdown_request_type shutdown_type{};
            switch (rq.shutdown().type()) {
            case tateyama::proto::core::request::ShutdownType::SHUTDOWN_TYPE_NOT_SET:
                shutdown_type = tateyama::session::shutdown_request_type::forceful;
                break;
            case tateyama::proto::core::request::ShutdownType::GRACEFUL:
                shutdown_type = tateyama::session::shutdown_request_type::graceful;
                break;
            case tateyama::proto::core::request::ShutdownType::FORCEFUL:
                shutdown_type = tateyama::session::shutdown_request_type::forceful;
                break;
            default: // error
                return false;
            }
            request_shutdown(shutdown_type);
            {
                std::lock_guard<std::mutex> lock(mtx_reqreses_);
                shutdown_response_.emplace(index, res);
            }
            return true;
        }

        case tateyama::proto::core::request::Request::kUpdateExpirationTime:
        {
            const auto& command = rq.update_expiration_time();
            if (command.expiration_time_opt_case() == tateyama::proto::core::request::UpdateExpirationTime::ExpirationTimeOptCase::kExpirationTime) {
                auto et = std::chrono::milliseconds{command.expiration_time()};
                if (refresh_timeout_ > et) {
                    et = refresh_timeout_;
                }
                if (et > max_refresh_timeout_) {
                    et = max_refresh_timeout_;
                }
                auto new_until_time = tateyama::session::session_context::expiration_time_type::clock::now() + et;
                if (auto expiration_time_opt = session_context_->expiration_time(); expiration_time_opt) {
                    if (expiration_time_opt.value() < new_until_time) {
                        session_context_->expiration_time(new_until_time);
                    }
                }
            } else {
                update_expiration_time();
            }
            tateyama::proto::core::response::UpdateExpirationTime rp{};
            rp.mutable_success();
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            return true;
        }

        default:
            // this request can not be handled here;
            return false;
        }
    }

    [[nodiscard]] bool shutdown_from_client() const noexcept {
        return complete_shutdown_from_client_;
    }

    void shutdown_complete() {
        std::vector<std::shared_ptr<tateyama::api::server::response>> targets{};
        {
            std::lock_guard<std::mutex> lock(mtx_reqreses_);
            for (auto&& e: shutdown_response_) {
                targets.emplace_back(e.second);
            }
            shutdown_response_.clear();
        }
        if (!targets.empty()) {
            tateyama::proto::core::response::Shutdown rp{};
            auto body = rp.SerializeAsString();
            for (auto&& e: targets) {
                e->body(body);
            }
            complete_shutdown_from_client_ = true;
        }
    }

    [[nodiscard]] bool register_reqres(std::size_t slot, const std::shared_ptr<tateyama::endpoint::common::request>& request, const std::shared_ptr<tateyama::endpoint::common::response>& response) noexcept {
        {
            std::lock_guard<std::mutex> lock(mtx_reqreses_);
            if (auto itr = reqreses_.find(slot); itr != reqreses_.end()) {
                reqreses_.erase(itr);  // shound not happen
            }
            reqreses_.emplace(slot, std::pair<std::shared_ptr<tateyama::endpoint::common::request>, std::shared_ptr<tateyama::endpoint::common::response>>(request, response));
        }
        if (request->get_blob_error() != request::blob_error::ok) {
            notify_client(response.get(), tateyama::proto::diagnostics::Code::OPERATION_DENIED, request->blob_error_message());
            return false;
        }
        return true;
    }

    void remove_reqres(std::size_t slot) noexcept {
        std::lock_guard<std::mutex> lock(mtx_reqreses_);
        if (auto&& itr = reqreses_.find(slot); itr != reqreses_.end()) {
            reqreses_.erase(itr);
        }
    }

    bool check_shutdown_request() {
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
        if (shutdown_response_.empty()) {
            return reqreses_.empty();
        }
        for (auto r_itr{reqreses_.begin()}, end{reqreses_.end()}; r_itr != end; r_itr++) {
            if (shutdown_response_.find(r_itr->first) == shutdown_response_.end()) {
                return false;
            }
        }
        return true;
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

    void update_expiration_time(bool force = false) {
        if (enable_timeout_) {
            auto new_until_time = tateyama::session::session_context::expiration_time_type::clock::now() + refresh_timeout_;
            if (force) {
                session_context_->expiration_time(new_until_time);
            } else if (auto expiration_time_opt = session_context_->expiration_time(); expiration_time_opt) {
                if (expiration_time_opt.value() < new_until_time) {
                    session_context_->expiration_time(new_until_time);
                }
            }
        }
    }
    bool is_expiration_time_over() {
        if (auto expiration_time_opt = session_context_->expiration_time(); expiration_time_opt) {
            bool rv = tateyama::session::session_context::expiration_time_type::clock::now() > expiration_time_opt.value();
            if (rv) {
                LOG_LP(INFO) << "expiration time over, session = " << resources_.session_id();
            }
            return rv;
        }
        return false;
    }
    inline tateyama::endpoint::common::resources& resources() {
        return resources_;
    }

private:
    tateyama::endpoint::common::resources resources_;
    const std::shared_ptr<tateyama::session::resource::session_context_impl> session_context_;
    bool enable_timeout_;
    std::chrono::seconds refresh_timeout_;
    std::chrono::seconds max_refresh_timeout_;
    authentication::resource::bridge* auth_;
    std::map<std::size_t, std::pair<std::shared_ptr<tateyama::endpoint::common::request>, std::shared_ptr<tateyama::endpoint::common::response>>> reqreses_{};
    std::mutex mtx_reqreses_{};
    std::map<std::size_t, std::shared_ptr<tateyama::api::server::response>> shutdown_response_{};
    bool cancel_requested_to_all_responses_{};
    bool dispose_done_{};
    bool complete_shutdown_from_client_{};

    void dump_message(std::ostream& os, std::string_view message) {
        for (auto&& c: message) {
            os << std::hex << std::setw(2) << std::setfill('0') << (static_cast<std::uint32_t>(c) & 0xffU) << " ";
        }
    }
};

}  // tateyama::endpoint::common
