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
#include <tateyama/loopback/buffered_response.h>
#include <tateyama/endpoint/loopback/loopback_response.h>

namespace tateyama::loopback {

buffered_response::buffered_response(std::shared_ptr<tateyama::api::server::response> response) {
    response_ = std::move(response);
}

std::size_t buffered_response::session_id() const noexcept {
    auto res = dynamic_cast<tateyama::common::loopback::loopback_response*>(response_.get());
    return res->session_id();
}

tateyama::api::server::response_code buffered_response::code() const noexcept {
    auto res = dynamic_cast<tateyama::common::loopback::loopback_response*>(response_.get());
    return res->code();
}

std::string_view buffered_response::body_head() const noexcept {
    auto res = dynamic_cast<tateyama::common::loopback::loopback_response*>(response_.get());
    return res->body_head();
}

std::string_view buffered_response::body() const noexcept {
    auto res = dynamic_cast<tateyama::common::loopback::loopback_response*>(response_.get());
    return res->body();
}

bool buffered_response::has_channel(std::string_view name) const noexcept {
    auto res = dynamic_cast<tateyama::common::loopback::loopback_response*>(response_.get());
    return res->has_channel(name);
}

std::vector<std::string> buffered_response::channel(std::string_view name) const {
    auto res = dynamic_cast<tateyama::common::loopback::loopback_response*>(response_.get());
    return res->channel(name);
}

} // namespace tateyama::loopback
