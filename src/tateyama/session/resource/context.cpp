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

#include "tateyama/session/resource/context.h"

namespace tateyama::session::resource {

session_context::session_context(session_info& info, session_variable_set variables) noexcept :
    info_(info), variables_(std::move(variables)) {
}

session_context::numeric_id_type session_context::numeric_id() const noexcept {
    return static_cast<numeric_id_type>(info_.id());
}

std::string_view session_context::symbolic_id() const noexcept {
    return info_.label();
}

session_info const& session_context::info() const noexcept {
    return info_;
}

session_variable_set& session_context::variables() noexcept {
    return variables_;
}

session_variable_set const& session_context::variables() const noexcept {
    return variables_;
}

shutdown_request_type session_context::shutdown_request() const noexcept {
    return shutdown_request_.load();
}

bool session_context::shutdown_request(shutdown_request_type type) noexcept {
    if (type == shutdown_request_type::nothing) {
        return false;
    }
    auto old = shutdown_request_.load();
    while (true) {
        if (old == type || old == shutdown_request_type::forceful) {
            return false;
        }
        if (shutdown_request_.compare_exchange_strong(old, type)) {
            return true;
        }
    }
}

}
