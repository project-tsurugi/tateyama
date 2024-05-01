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
#include "stream_worker.h"

#ifdef ENABLE_ALTIMETER
#include "tateyama/endpoint/altimeter/logger.h"
#endif
#include "tateyama/endpoint/stream/stream_request.h"
#include "tateyama/endpoint/stream/stream_response.h"

namespace tateyama::endpoint::stream::bootstrap {

void stream_worker::do_work()
{
    while (true) {
        std::uint16_t slot{};
        std::string payload{};
        switch (session_stream_->await(slot, payload)) {

        case tateyama::endpoint::stream::stream_socket::await_result::payload:
        {
            stream_request request_obj{*session_stream_, payload, database_info_, session_info_, session_store_};
            stream_response response_obj{session_stream_, slot, [](){}};

            if (decline_) {
                notify_of_decline(&response_obj);
                if (session_stream_->await(slot, payload) == tateyama::endpoint::stream::stream_socket::await_result::payload) {
                    LOG_LP(INFO) << "illegal procedure (receive a request in spite of a decline case)";  // should not reach here
                } else {
                    VLOG_LP(log_trace) << "session termination due to reaching the maximum number of sessions: session_id = " << std::to_string(session_id_);
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

        default:
            session_stream_->close();
            VLOG_LP(log_trace) << "received shutdown request: session_id = " << std::to_string(session_id_);
            return;
        }
        break;
    }

    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started " << std::to_string(session_id_);
#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_start(database_info_, session_info_, session_store_);
#endif
    while(true) {
        std::uint16_t slot{};
        std::string payload{};

        switch (session_stream_->await(slot, payload)) {
        case tateyama::endpoint::stream::stream_socket::await_result::payload:
        {
            auto request = std::make_shared<stream_request>(*session_stream_, payload, database_info_, session_info_, session_store_);
            auto response = std::make_shared<stream_response>(session_stream_, slot, [this, slot](){remove_reqres(slot);});
            register_reqres(slot,
                            std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                            std::dynamic_pointer_cast<tateyama::endpoint::common::response>(response));
            switch (request->service_id()) {
            case tateyama::framework::service_id_endpoint_broker:
                if (endpoint_service(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                     std::dynamic_pointer_cast<tateyama::api::server::response>(response),
                                     slot)) {
                    continue;
                }
                VLOG_LP(log_info) << "terminate worker because endpoint service returns an error";
                break;

            case tateyama::framework::service_id_routing:
                if (routing_service_chain(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                          std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                    continue;
                }
                if (service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                              std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                    continue;
                }
                VLOG_LP(log_info) << "terminate worker because service returns an error";
                break;

            default:
                if (!is_shuttingdown()) {
                    if(service_(std::dynamic_pointer_cast<tateyama::api::server::request>(request),
                                std::dynamic_pointer_cast<tateyama::api::server::response>(response))) {
                        continue;
                    }
                    VLOG_LP(log_info) << "terminate worker because service returns an error";
                } else {
                    notify_client(response.get(), tateyama::proto::diagnostics::SESSION_CLOSED, "");
                    continue;
                }
            }
            request = nullptr;
            response = nullptr;
            break;
        }

        case tateyama::endpoint::stream::stream_socket::await_result::timeout:
            care_reqreses();
            if (is_shuttingdown() && is_completed()) {
                VLOG_LP(log_trace) << "received and completed shutdown request: session_id = " << std::to_string(session_id_);
                break;
            }
            continue;

        case tateyama::endpoint::stream::stream_socket::await_result::termination_request:
            dispose_session_store();
            session_stream_->send_session_bye_ok();
            continue;

        default:  // some error
            dispose_session_store();
            break;
        }
        break;
    }
    session_stream_->close();

#ifdef ENABLE_ALTIMETER
    tateyama::endpoint::altimeter::session_end(database_info_, session_info_);
#endif
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished " << session_id_;
}

bool stream_worker::terminate(tateyama::session::shutdown_request_type type) {
    VLOG_LP(log_trace) << "send terminate request: session_id = " << std::to_string(session_id_);

    return request_shutdown(type);
}

}
