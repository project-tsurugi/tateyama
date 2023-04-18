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

#include "tateyama/loopback/loopback_client.h"

#include <tateyama/endpoint/loopback/bootstrap/loopback_endpoint.h>

namespace tateyama::loopback {

loopback_client::loopback_client() {
    endpoint_ = std::make_shared<tateyama::framework::loopback_endpoint>();
}

std::shared_ptr<tateyama::framework::endpoint> loopback_client::endpoint() const noexcept {
    return endpoint_;
}

buffered_response loopback_client::request(std::size_t session_id, std::size_t service_id, std::string_view payload) {
    auto endpoint = dynamic_cast<tateyama::framework::loopback_endpoint*>(endpoint_.get());
    return endpoint->request(session_id, service_id, payload);
}

buffered_response loopback_client::request(std::size_t session_id, std::size_t service_id, std::string_view payload,
        buffered_response&&) {
    auto endpoint = dynamic_cast<tateyama::framework::loopback_endpoint*>(endpoint_.get());
    // FIXME
    // return endpoint->request(session_id, service_id, payload, recycle);
    return endpoint->request(session_id, service_id, payload);
}

} // namespace tateyama::loopback
