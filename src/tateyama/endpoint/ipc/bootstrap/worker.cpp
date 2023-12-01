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
#include <exception>

#include "worker.h"

#include <tateyama/endpoint/ipc/ipc_request.h>
#include <tateyama/endpoint/ipc/ipc_response.h>
#include <tateyama/proto/diagnostics.pb.h>

namespace tateyama::server {

void Worker::run()
{
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started "
        << session_id_;
    while(true) {
        try {
            auto h = request_wire_container_->peep(true);
            if (terminate_requested_.load()) {
                VLOG_LP(log_trace) << "received terminate request: session_id = " << std::to_string(session_id_);
                break;
            }
            if (h.get_length() == 0 && h.get_idx() == tateyama::common::wire::message_header::not_use) { break; }
            auto request = std::make_shared<tateyama::common::wire::ipc_request>(*wire_, h);
            auto response = std::make_shared<tateyama::common::wire::ipc_response>(wire_, h.get_idx(), [this](tateyama::common::wire::ipc_response* entry){ std::lock_guard<std::mutex> lock(mtx_response_set_); incomplete_responses_.erase(entry); });

            {
                std::lock_guard<std::mutex> lock(mtx_response_set_);
                incomplete_responses_.emplace(response.get());
            }
            service_(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                     static_cast<std::shared_ptr<tateyama::api::server::response>>(std::move(response)));
            request->dispose();
            request = nullptr;
        } catch (std::runtime_error &e) {
            LOG_LP(ERROR) << e.what();
            break;
        }
    }
    clean_up_();
    VLOG_LP(log_trace) << "destroy session wire: session_id = " << std::to_string(session_id_);
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished "
        << session_id_;
    terminated_ = true;
}

void Worker::terminate() {
    VLOG_LP(log_trace) << "send terminate request: session_id = " << std::to_string(session_id_);
    terminate_requested_.store(true);
    wire_->terminate();

    // terminate requests that has not been responded yet.
    tateyama::proto::diagnostics::Record rec{};
    rec.set_code(tateyama::proto::diagnostics::Code::ILLEGAL_STATE);
    rec.set_message("tsurugidb is shutting down now");

    {
        std::lock_guard<std::mutex> lock(mtx_response_set_);
        for (auto&& e : incomplete_responses_) {
            e->error(rec);
        }
    }
}

}  // tateyama::server
