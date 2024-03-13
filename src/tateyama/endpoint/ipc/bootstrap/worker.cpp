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

#ifdef ENABLE_ALTIMETER
#include "tateyama/endpoint/altimeter/logger.h"
#endif
#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

namespace tateyama::endpoint::ipc::bootstrap {

void Worker::run() {
    while(true) {
        auto hdr = request_wire_container_->peep();
        if (hdr.get_length() == 0 && hdr.get_idx() == tateyama::common::wire::message_header::null_request) {
            if (request_wire_container_->terminate_requested()) {
                VLOG_LP(log_trace) << "received shutdown request: session_id = " << std::to_string(session_id_);
                return;
            }
            continue;
        }

        ipc_request request_obj{*wire_, hdr, database_info_, session_info_};
        ipc_response response_obj{wire_, hdr.get_idx(), [](){}};
        if (! handshake(static_cast<tateyama::api::server::request*>(&request_obj), static_cast<tateyama::api::server::response*>(&response_obj))) {
            return;
        }
        break;
    }

    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started " << session_id_;
#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_start(database_info_, session_info_);
#endif
    while(true) {
        try {
            auto h = request_wire_container_->peep();
            if (h.get_length() == 0 && h.get_idx() == tateyama::common::wire::message_header::null_request) {
                if (handle_shutdown()) {
                    VLOG_LP(log_trace) << "received and completed shutdown request: session_id = " << std::to_string(session_id_);
                    break;
                }
                if (request_wire_container_->terminate_requested()) {
                    shutdown_request(tateyama::session::shutdown_request_type::forceful);
                }
                continue;
            }

            auto request = std::make_shared<ipc_request>(*wire_, h, database_info_, session_info_);
            std::size_t index = h.get_idx();
            auto response = std::make_shared<ipc_response>(wire_, h.get_idx(), [this, index](){remove_response(index);});
            if (request->service_id() != tateyama::framework::service_id_endpoint_broker) {
                if (!handle_shutdown()) {
                    register_response(index, static_cast<std::shared_ptr<tateyama::endpoint::common::response>>(response));
                    if (!service_(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                                  static_cast<std::shared_ptr<tateyama::api::server::response>>(std::move(response)))) {
                        VLOG_LP(log_info) << "terminate worker because service returns an error";
                        break;
                    }
                } else {
                    notify_client(response.get(), tateyama::proto::diagnostics::SESSION_CLOSED, "");
                    if (!has_incomplete_response()) {
                        break;
                    }
                }
            } else {
                if (!endpoint_service(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                                      static_cast<std::shared_ptr<tateyama::api::server::response>>(std::move(response)),
                                      index)) {
                    VLOG_LP(log_info) << "terminate worker because endpoint service returns an error";
                    break;
                }
            }
            request->dispose();
            request = nullptr;
            wire_->get_garbage_collector()->dump();
        } catch (std::runtime_error &e) {
            LOG_LP(ERROR) << e.what();
            break;
        }
    }
    VLOG_LP(log_trace) << "destroy session wire: session_id = " << std::to_string(session_id_);
#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_end(database_info_, session_info_);
#endif
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished " << session_id_;
}

bool Worker::terminate(tateyama::session::shutdown_request_type type) {
    VLOG_LP(log_trace) << "send terminate request: session_id = " << std::to_string(session_id_);

    auto rv = shutdown_request(type);
    wire_->notify();
    return rv;
}

}
