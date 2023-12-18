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
#include "stream_worker.h"

#include "tateyama/endpoint/stream/stream_request.h"
#include "tateyama/endpoint/stream/stream_response.h"

namespace tateyama::endpoint::stream::bootstrap {

void stream_worker::run(const std::function<void(void)>& clean_up)
{
    {
        std::uint16_t slot{};
        std::string payload{};
        if (!session_stream_->await(slot, payload)) {
            session_stream_->close();
            return;
        }

        stream_request request_obj{*session_stream_, payload, database_info_, session_info_};
        stream_response response_obj{session_stream_, slot};

        if (decline_) {
            notify_of_session_limit(&response_obj);
            clean_up();
            return;
        }

        if (! handshake(static_cast<tateyama::api::server::request*>(&request_obj), static_cast<tateyama::api::server::response *>(&response_obj))) {
            return;
        }
        session_stream_->change_slot_size(max_result_sets_);
    }

    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started " << session_id_;
    while(true) {
        std::uint16_t slot{};
        std::string payload{};
        if (!session_stream_->await(slot, payload)) {
            session_stream_->close();
            break;
        }

        auto request = std::make_shared<stream_request>(*session_stream_, payload, database_info_, session_info_);
        auto response = std::make_shared<stream_response>(session_stream_, slot);
        service_(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                 static_cast<std::shared_ptr<tateyama::api::server::response>>(std::move(response)));
        request = nullptr;
    }
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished " << session_id_;
}

}
