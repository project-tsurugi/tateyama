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
#include <tateyama/loopback/buffered_response.h>

namespace tateyama::loopback {

buffered_response::buffered_response(std::size_t session_id, tateyama::api::server::response_code code,
        std::string_view body_head, std::string_view body,
        std::map<std::string, std::vector<std::string>, std::less<>> &&data_map) :
        session_id_(session_id), code_(code), body_head_(body_head), body_(body), data_map_(std::move(data_map)) {
}

std::size_t buffered_response::session_id() const noexcept {
    return session_id_;
}

tateyama::api::server::response_code buffered_response::code() const noexcept {
    return code_;
}

std::string_view buffered_response::body_head() const noexcept {
    return body_head_;
}

std::string_view buffered_response::body() const noexcept {
    return body_;
}

bool buffered_response::has_channel(std::string_view name) const noexcept {
    return data_map_.find(name) != data_map_.cend();
}

std::vector<std::string> const& buffered_response::channel(std::string_view name) const {
    auto it = data_map_.find(name);
    if (it != data_map_.cend()) {
        return it->second;
    }
    throw std::invalid_argument("invalid channel name: " + std::string { name });
}

} // namespace tateyama::loopback
