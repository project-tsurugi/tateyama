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
#include <functional>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/status/resource/bridge.h>
#include "tateyama/logging_helper.h"

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

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

    worker_common(connection_type con, std::size_t session_id, std::string_view conn_info)
        : connection_type_(con),
          session_id_(session_id),
          session_info_(session_id, connection_label(con), conn_info) {
    }
    worker_common(connection_type con, std::size_t id) : worker_common(con, id, "") {
    }
    void invoke(std::function<void(void)> func) {
        task_ = std::packaged_task<void()>(std::move(func));
        future_ = task_.get_future();
        thread_ = std::thread(std::move(task_));
    }
    auto wait_for() {
        return future_.wait_for(std::chrono::seconds(0));
    }

protected:
    const connection_type connection_type_; // NOLINT
    const std::size_t session_id_;          // NOLINT
    session_info_impl session_info_;        // NOLINT
    std::string connection_info_{};         // NOLINT
    std::size_t max_result_sets_{};         // NOLINT

    // for future
    std::packaged_task<void()> task_;       // NOLINT
    std::future<void> future_;              // NOLINT
    std::thread thread_{};                  // NOLINT

    bool handshake(tateyama::api::server::request* req, tateyama::api::server::response* res) {
        if (req->service_id() != tateyama::framework::service_id_endpoint_broker) {
            LOG(ERROR) << "request has invalid service id";
            return false;
        }
        auto data = req->payload();
        tateyama::proto::endpoint::request::Request rq{};
        if(! rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            VLOG(log_error) << "request parse error";
            return false;
        }
        if(rq.command_case() != tateyama::proto::endpoint::request::Request::kHandshake) {
            LOG(ERROR) << "bad request (handshake in endpoint): " << rq.command_case();
            return false;
        }
        auto ci = rq.handshake().client_infomation();
        session_info_.label(ci.connection_label());
        session_info_.application_name(ci.application_name());
        session_info_.user_name(ci.user_name());
        if (connection_type_ == connection_type::ipc) {
            session_info_.connection_information(ci.connection_information());
        }
        max_result_sets_ = ci.maximum_concurrent_result_sets();  // for Stream

        VLOG_LP(log_trace) << session_info_;  //NOLINT

        tateyama::proto::endpoint::response::Handshake rp{};
        auto rs = rp.mutable_success();
        rs->set_session_id(session_id_);
        auto body = rp.SerializeAsString();
        res->body(body);
        rp.clear_success();
        return true;
    }

    void notify_of_session_limit(tateyama::api::server::response* res) {
        tateyama::proto::endpoint::response::Handshake rp{};
        auto rs = rp.mutable_unknown_error();
        rs->set_code(tateyama::proto::endpoint::error::Code::ENDPOINT_SESSION_LIMIT_EXCEPTION);
        auto body = rp.SerializeAsString();
        res->body(body);
        rp.clear_success();
    }

private:
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
};

}  // tateyama::endpoint::common
