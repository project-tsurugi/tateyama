/*
 * Copyright 2019-2022 tsurugi project.
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

#include <tateyama/endpoint/stream/stream_request.h>
#include <tateyama/endpoint/stream/stream_response.h>

namespace tateyama::server {

void stream_worker::run()
{
    std::string session_name = std::to_string(session_id_);
    if (session_stream_->wait_hello(session_name)) {
        while(true) {
            std::uint16_t slot{};
            std::string payload{};
            if (!session_stream_->await(slot, payload)) {
                session_stream_->close();
                break;
            }

            auto request = std::make_shared<tateyama::common::stream::stream_request>(*session_stream_, payload);
            auto response = std::make_shared<tateyama::common::stream::stream_response>(*request, slot);
            service_(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                     static_cast<std::shared_ptr<tateyama::api::server::response>>(std::move(response)));
            request = nullptr;
        }
    }
}

}  // tateyama::server
