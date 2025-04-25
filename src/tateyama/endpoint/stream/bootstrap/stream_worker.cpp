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

#include "stream_worker.h"

#ifdef ENABLE_ALTIMETER
#include "tateyama/endpoint/altimeter/logger.h"
#endif
#include "tateyama/endpoint/stream/stream_request.h"
#include "tateyama/endpoint/stream/stream_response.h"

namespace tateyama::endpoint::stream::bootstrap {

void stream_worker::run()  // NOLINT(readability-function-cognitive-complexity)
{
    while (true) {
        std::uint16_t slot{};
        std::string payload{};
        switch (session_stream_->await(slot, payload)) {

        case tateyama::endpoint::stream::stream_socket::await_result::payload:
        {
            stream_request request_obj{*session_stream_, payload,  resources(), local_id_++, conf_};
            stream_response response_obj{session_stream_, slot, [](){}, conf_};

            if (decline_) {
                notify_of_decline(&response_obj);
                if (session_stream_->await(slot, payload) == tateyama::endpoint::stream::stream_socket::await_result::payload) {
                    LOG_LP(INFO) << "illegal procedure (receive a request in spite of a decline case)";  // should not reach here
                } else {
                    VLOG_LP(log_trace) << "session termination due to reaching the maximum number of sessions: session_id = " << std::to_string(session_id());
                }
                session_stream_->close();
                return;
            }

            if (! handshake(static_cast<tateyama::api::server::request*>(&request_obj), static_cast<tateyama::api::server::response *>(&response_obj))) {
                if (session_stream_->await(slot, payload) == tateyama::endpoint::stream::stream_socket::await_result::payload) {
                    LOG_LP(INFO) << "illegal termination of the session due to handshake error";  // should not reach here
                }
                session_stream_->close();
                return;
            }

            session_stream_->change_slot_size(max_result_sets_);
            break;
        }

        case tateyama::endpoint::stream::stream_socket::await_result::timeout:
            continue;

        case tateyama::endpoint::stream::stream_socket::await_result::socket_closed:
            session_stream_->close();
            VLOG_LP(log_trace) << "socket has been closed by the client: session_id = " << std::to_string(session_id());
            return;

        case tateyama::endpoint::stream::stream_socket::await_result::termination_request:
            VLOG_LP(log_trace) << "received shutdown request: session_id = " << std::to_string(session_id());
            if (shutdown_from_client()) {
                session_stream_->send_session_bye_ok();
            }
            return;

        default:
            session_stream_->close();
            VLOG_LP(log_trace) << "detects illegal state: session_id = " << std::to_string(session_id());
            return;
        }
        break;
    }

    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started " << std::to_string(session_id());
#ifdef ENABLE_ALTIMETER
    const std::chrono::time_point session_start_time = std::chrono::high_resolution_clock::now();
    tateyama::endpoint::altimeter::session_start(conf_.database_info(), resources().session_info());
#endif
    bool notiry_expiration_time_over{};
    while(true) {
        std::uint16_t slot{};
        std::string payload{};

        switch (session_stream_->await(slot, payload)) {
        case tateyama::endpoint::stream::stream_socket::await_result::payload:
        {
            update_expiration_time();
            auto request = std::make_shared<stream_request>(*session_stream_, payload,  resources(), local_id_++, conf_);
            switch (request->service_id()) {
            case tateyama::framework::service_id_endpoint_broker:
            {
                auto response = std::make_shared<stream_response>(session_stream_, slot, [](){}, conf_);
                // currently cancel request only
                if (endpoint_service(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                     std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response),
                                     slot)) {
                    continue;
                }
                VLOG_LP(log_info) << "terminate worker because endpoint service returns an error";
                break;
            }
            case tateyama::framework::service_id_routing:
            {
                pending_clear_count_++;
                auto response = std::make_shared<stream_response>(session_stream_, slot, [this, slot](){remove_reqres(slot); pending_clear_count_--;}, conf_);
                if (!register_reqres(slot,
                                     std::dynamic_pointer_cast<tateyama::endpoint::common::request>(request),
                                     std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response))) {
                    continue;  // error has been notified to the client
                }
                if (routing_service_chain(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                          std::dynamic_pointer_cast<tateyama::api::server::response>(response),
                                          slot)) {
                    care_reqreses();
                    if (check_shutdown_request() && is_completed()) {
                        shutdown_complete();
                        VLOG_LP(log_trace) << "received and completed shutdown request: session_id = " << std::to_string(session_id());
                    }
                    continue;
                }
                if (service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                             std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                    continue;
                }
                VLOG_LP(log_info) << "terminate worker because service returns an error";
                break;
            }
            default:
            {
                pending_clear_count_++;
                auto response = std::make_shared<stream_response>(session_stream_, slot, [this, slot](){remove_reqres(slot); pending_clear_count_--;}, conf_);
                if (!check_shutdown_request()) {
                    if (!register_reqres(slot,
                                         std::dynamic_pointer_cast<tateyama::endpoint::common::request>(request),
                                         std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response))) {
                        continue;  // error has been notified to the client
                    }
                    if(service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                        continue;
                    }
                    VLOG_LP(log_info) << "terminate worker because service returns an error";
                } else {
                    notify_client(response.get(), tateyama::proto::diagnostics::SESSION_CLOSED, "this session is already shutdown");
                    continue;
                }
            }
            }
            request = nullptr;
            break;
        }

        case tateyama::endpoint::stream::stream_socket::await_result::timeout:
            care_reqreses();
            if (check_shutdown_request() && is_completed()) {
                VLOG_LP(log_trace) << "received and completed shutdown request: session_id = " << std::to_string(session_id());
                shutdown_complete();
                if (!shutdown_from_client()) {
                    break;
                }
            }
            if (is_expiration_time_over() && !notiry_expiration_time_over) {
                request_shutdown(tateyama::session::shutdown_request_type::forceful);
                notiry_expiration_time_over = true;
            }
            continue;

        case tateyama::endpoint::stream::stream_socket::await_result::termination_request:
            if (shutdown_from_client()) {
                session_stream_->send_session_bye_ok();
                break;
            }
            request_shutdown(tateyama::session::shutdown_request_type::forceful);
            session_stream_->send_session_bye_ok();
            continue;

        case tateyama::endpoint::stream::stream_socket::await_result::socket_closed:
            VLOG_LP(log_trace) << "socket has been closed by the client: session_id = " << std::to_string(session_id());
            break;

        default:  // some error
            VLOG_LP(log_trace) << "detects illegal state: session_id = " << std::to_string(session_id());
            break;
        }
        break;
    }
    session_stream_->close();

#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_end(conf_.database_info(), resources().session_info(), std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - session_start_time).count());
#endif
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished " << session_id();
}

bool stream_worker::terminate(tateyama::session::shutdown_request_type type) {
    VLOG_LP(log_trace) << "send terminate request: session_id = " << std::to_string(session_id());

    return request_shutdown(type);
}

}
