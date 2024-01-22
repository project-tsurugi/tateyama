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

#include "loopback_endpoint.h"

#include <tateyama/endpoint/loopback/loopback_request.h>
#include <tateyama/endpoint/loopback/loopback_response.h>

namespace tateyama::endpoint::loopback {

tateyama::loopback::buffered_response loopback_endpoint::request(std::size_t session_id, std::size_t service_id,
        std::string_view payload) {
    auto request = std::make_shared<loopback_request>(session_id, service_id, payload);
    auto response = std::make_shared<loopback_response>();

    if (service_->operator ()(std::move(request), response)) {
        return tateyama::loopback::buffered_response { response->session_id(), response->body_head(),
                response->body(), response->release_all_committed_data() };
    } else {
        return tateyama::loopback::buffered_response { response->session_id(), response->error() };
    }
}

} // namespace tateyama::endpoint::loopback
