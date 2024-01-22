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
#include <tateyama/proto/diagnostics.pb.h>

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

    bool handshake(tateyama::api::server::request* req, tateyama::api::server::response* res) {
        if (req->service_id() != tateyama::framework::service_id_endpoint_broker) {
            std::string error_message{"request has invalid service id"};
            LOG(ERROR) << error_message;
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }
        auto data = req->payload();
        tateyama::proto::endpoint::request::Request rq{};
        if(! rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string error_message{"request parse error"};
            LOG(ERROR) << error_message;
            notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, error_message);
            return false;
        }
        if(rq.command_case() != tateyama::proto::endpoint::request::Request::kHandshake) {
            std::stringstream ss;
            ss << "bad request (handshake in endpoint): " << rq.command_case();
            LOG(ERROR) << ss.str();
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
                LOG(ERROR) << ss.str();
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
                return false;
            }
            session_info_.connection_information(wi.ipc_information().connection_information());
            break;
        case connection_type::stream:
            if(wi.wire_information_case() != tateyama::proto::endpoint::request::WireInformation::kStreamInformation) {
                std::stringstream ss;
                ss << "bad wire information (handshake in endpoint): " << wi.wire_information_case();
                LOG(ERROR) << ss.str();
                notify_client(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, ss.str());
                return false;
            }
            max_result_sets_ = wi.stream_information().maximum_concurrent_result_sets();  // for Stream
            break;
        default:  // shouldn't happen
            std::stringstream ss;
            ss << "illegal connection type: " << static_cast<std::uint32_t>(connection_type_);
            LOG(ERROR) << ss.str();
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
