/*
 * Copyright 2018-2023 tsurugi project.
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

namespace tateyama::framework {

tateyama::loopback::buffered_response loopback_endpoint::request(std::size_t session_id, std::size_t service_id,
        std::string_view payload) {
    auto request = std::make_shared<tateyama::common::loopback::loopback_request>(session_id, service_id, payload);
    auto response = std::make_shared<tateyama::common::loopback::loopback_response>();

    bool ok = service_->operator ()(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
            static_cast<std::shared_ptr<tateyama::api::server::response>>(response));
    if (!ok) {
        throw std::invalid_argument("unknown service_id " + std::to_string(service_id));
    }

    tateyama::loopback::buffered_response bufres { };
    bufres.update(response->session_id(), response->code(), response->body_head(), response->body(),
            response->all_committed_data());
    return bufres;
}

} // namespace tateyama::framework
