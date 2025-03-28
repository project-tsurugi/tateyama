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
#include <chrono>
#include <exception>

#include "ipc_worker.h"

#ifdef ENABLE_ALTIMETER
#include "tateyama/endpoint/altimeter/logger.h"
#endif

namespace tateyama::endpoint::ipc::bootstrap {

void ipc_worker::run() {  // NOLINT(readability-function-cognitive-complexity)
    tateyama::common::wire::message_header hdr{};
    while(true) {
        try {
            hdr = request_wire_container_->peep();
        } catch (std::exception &ex) {
            continue;
        }
        if (hdr.get_length() == 0 && hdr.get_idx() == tateyama::common::wire::message_header::terminate_request) {
            VLOG_LP(log_trace) << "received shutdown request: session_id = " << std::to_string(session_id());
            return;
        }

        ipc_request request_obj{*wire_, hdr, resources(), local_id_++, conf_};
        ipc_response response_obj{wire_, hdr.get_idx(), [](){}, conf_};
        if (! handshake(static_cast<tateyama::api::server::request*>(&request_obj), static_cast<tateyama::api::server::response*>(&response_obj))) {
            return;
        }
        break;
    }

    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started " << session_id();
#ifdef ENABLE_ALTIMETER
    const std::chrono::time_point session_start_time = std::chrono::steady_clock::now();
    tateyama::endpoint::altimeter::session_start(conf_.database_info(), resources().session_info());
#endif
    bool notiry_expiration_time_over{};
    while(true) {
        try {
            hdr = request_wire_container_->peep();
        } catch (std::exception &ex) {
            care_reqreses();
            if (check_shutdown_request() && is_completed()) {
                VLOG_LP(log_trace) << "terminate worker thread for session " << session_id() << ", as it has received a shutdown request";
                break;  // break the while loop
            }
            if (is_expiration_time_over() && !notiry_expiration_time_over) {
                wire_->expiration_time_over();
                request_shutdown(tateyama::session::shutdown_request_type::forceful);
                notiry_expiration_time_over = true;
            }
            continue;
        }
        try {
            if (hdr.get_length() == 0 && hdr.get_idx() == tateyama::common::wire::message_header::terminate_request) {
                request_shutdown(tateyama::session::shutdown_request_type::forceful);
                care_reqreses();
                if (check_shutdown_request() && is_completed()) {
                    VLOG_LP(log_trace) << "terminate worker thread for session " << session_id() << ", as disconnection is requested and the subsequent shutdown process is completed";
                    break;  // break the while loop
                }
                VLOG_LP(log_trace) << "shutdown for session " << session_id() << " is to be delayed";
                continue;
            }

            update_expiration_time();
            auto request = std::make_shared<ipc_request>(*wire_, hdr, resources(), local_id_++, conf_);
            std::size_t index = hdr.get_idx();
            bool exit_frag = false;
            switch (request->service_id()) {
            case tateyama::framework::service_id_endpoint_broker:
            {
                auto response = std::make_shared<ipc_response>(wire_, hdr.get_idx(), [](){}, conf_);
                // currently cancel request only
                if (!endpoint_service(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                      std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response),
                                      index)) {
                    VLOG_LP(log_info) << "terminate worker because endpoint service returns an error";
                    exit_frag = true;
                }
                break;  // break the switch, and set exit_flag true to break the while loop
            }
            case tateyama::framework::service_id_routing:
            {
                auto response = std::make_shared<ipc_response>(wire_, hdr.get_idx(), [this, index](){remove_reqres(index);}, conf_);
                if (!register_reqres(index,
                                    std::dynamic_pointer_cast<tateyama::endpoint::common::request>(request),
                                    std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response))) {
                    continue;  // error has been notified to the client
                }
                if (routing_service_chain(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                          std::dynamic_pointer_cast<tateyama::api::server::response>(response),
                                          index)) {
                    care_reqreses();
                    if (check_shutdown_request() && is_completed()) {
                        VLOG_LP(log_trace) << "received and completed shutdown request: session_id = " << std::to_string(session_id());
                        exit_frag = true;
                    }
                    break;  // break the switch, and set exit_flag true to break the while loop
                }
                if (!service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                              std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                    VLOG_LP(log_info) << "terminate worker because service returns an error";
                    exit_frag = true;
                }
                break;  // break the switch, and set exit_flag true to break the while loop
            }
            default:
            {
                auto response = std::make_shared<ipc_response>(wire_, hdr.get_idx(), [this, index](){remove_reqres(index);}, conf_);
                if (!check_shutdown_request()) {
                    if (!register_reqres(index,
                                         std::dynamic_pointer_cast<tateyama::endpoint::common::request>(request),
                                         std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response))) {
                        continue;  // error has been notified to the client
                    }
                    if (!service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                  std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                        VLOG_LP(log_info) << "terminate worker because service returns an error";
                        exit_frag = true;
                    }
                } else {
                    notify_client(response.get(), tateyama::proto::diagnostics::SESSION_CLOSED, "this session is already shutdown");
                }
                break;  // break the switch, and set exit_flag true to break the while loop
            }
            }
            if (exit_frag) {
                break;  // break the while loop
            }
            request->dispose();
            request = nullptr;
            wire_->get_garbage_collector()->dump();
        } catch (std::exception &e) {
            LOG_LP(ERROR) << e.what();
            break;  // break the while loop
        }
    }
    VLOG_LP(log_trace) << "destroy session wire: session_id = " << std::to_string(session_id());
#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_end(conf_.database_info(), resources().session_info(), std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - session_start_time).count());
#endif
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished " << session_id();
}

// Processes shutdown requests from outside the communication partner.
bool ipc_worker::terminate(tateyama::session::shutdown_request_type type) {
    VLOG_LP(log_trace) << "send terminate request: session_id = " << std::to_string(session_id());

    auto rv = request_shutdown(type);
    wire_->get_request_wire()->notify();
    return rv;
}

}
