/*
 * Copyright 2019-2023 tsurugi project.
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
#include "worker.h"

#include <tateyama/endpoint/ipc/ipc_request.h>
#include <tateyama/endpoint/ipc/ipc_response.h>

namespace tateyama::server {

void Worker::run()
{
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started "
        << session_id_;
    while(true) {
        auto h = request_wire_container_->peep(true);
        if (h.get_length() == 0 && h.get_idx() == tateyama::common::wire::message_header::not_use) { break; }
        auto request = std::make_shared<tateyama::common::wire::ipc_request>(*wire_, h);
        auto response = std::make_shared<tateyama::common::wire::ipc_response>(*request, h.get_idx());

        service_(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                 static_cast<std::shared_ptr<tateyama::api::server::response>>(std::move(response)));
        request->dispose();
        request = nullptr;
        if (wire_->is_session_closed()) { break; }
    }
    clean_up_();
    VLOG_LP(log_trace) << "destroy session wire: session_id = " << std::to_string(session_id_);
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:finished "
        << session_id_;
}

}  // tateyama::server
