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
#include <exception>

#include "ipc_worker.h"

#ifdef ENABLE_ALTIMETER
#include "tateyama/endpoint/altimeter/logger.h"
#endif
#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

namespace tateyama::endpoint::ipc::bootstrap {

void Worker::do_work() {
    tateyama::common::wire::message_header hdr{};
    while(true) {
        try {
            hdr = request_wire_container_->peep();
        } catch (std::runtime_error &ex) {
            continue;
        }
        if (hdr.get_length() == 0 && hdr.get_idx() == tateyama::common::wire::message_header::terminate_request) {
            VLOG_LP(log_trace) << "received shutdown request: session_id = " << std::to_string(session_id_);
            return;
        }

        ipc_request request_obj{*wire_, hdr, database_info_, session_info_, session_store_};
        ipc_response response_obj{wire_, hdr.get_idx(), [](){}};
        if (! handshake(static_cast<tateyama::api::server::request*>(&request_obj), static_cast<tateyama::api::server::response*>(&response_obj))) {
            return;
        }
        break;
    }

    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started " << session_id_;
#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_start(database_info_, session_info_, session_store_);
#endif
    while(true) {
        try {
            hdr = request_wire_container_->peep();
        } catch (std::runtime_error &ex) {
            care_reqreses();
            if (is_shuttingdown() && is_completed()) {
                wire_->get_response_wire().notify_shutdown();
                VLOG_LP(log_info) << "terminate worker because shutdown completed";
                break;
            }
            continue;
        }
        try {
            if (hdr.get_length() == 0 && hdr.get_idx() == tateyama::common::wire::message_header::terminate_request) {
                dispose_session_store();
                request_shutdown(tateyama::session::shutdown_request_type::forceful);
                care_reqreses();
                if (is_shuttingdown() && is_completed()) {
                    wire_->get_response_wire().notify_shutdown();
                    VLOG_LP(log_info) << "terminate worker because shutdown completed";
                    break;
                }
                continue;
            }

            auto request = std::make_shared<ipc_request>(*wire_, hdr, database_info_, session_info_, session_store_);
            std::size_t index = hdr.get_idx();
            auto response = std::make_shared<ipc_response>(wire_, hdr.get_idx(), [this, index](){remove_reqres(index);});
            register_reqres(index,
                            std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                            std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response));
            if (request->service_id() != tateyama::framework::service_id_endpoint_broker) {
                if (!is_shuttingdown()) {
                    if (!service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                  std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                        VLOG_LP(log_info) << "terminate worker because service returns an error";
                        break;
                    }
                } else {
                    notify_client(response.get(), tateyama::proto::diagnostics::SESSION_CLOSED, "");
                }
            } else {
                if (!endpoint_service(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                      std::dynamic_pointer_cast<tateyama::api::server::response>(response),
                                      index)) {
                    VLOG_LP(log_info) << "terminate worker because endpoint service returns an error";
                    break;
                }
            }
            request->dispose();
            request = nullptr;
            response = nullptr;
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

    auto rv = request_shutdown(type);
    wire_->notify_shutdown();
    return rv;
}

}
