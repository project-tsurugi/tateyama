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

#include "tateyama/session/variable_set.h"

namespace tateyama::session {

session_variable_set::session_variable_set() noexcept = default;

session_variable_set::session_variable_set(std::vector<std::tuple<std::string, variable_type, value_type>> declarations) {
    for(auto& e: declarations) {
        variable_set_.insert(std::make_pair(std::get<0>(e), std::make_pair(std::get<1>(e), std::get<2>(e))));
    }
}

bool session_variable_set::exists(std::string_view name) const noexcept {
    auto it = variable_set_.find(std::string(name));
    return it != variable_set_.end();
}

std::optional<session_variable_set::variable_type> session_variable_set::type(std::string_view name) const noexcept {
    auto it = variable_set_.find(std::string(name));
    if (it != variable_set_.end()) {
        return it->second.first;
    }
    return std::nullopt;
}

session_variable_set::value_type session_variable_set::get(std::string_view name) const {
    auto it = variable_set_.find(std::string(name));
    if (it != variable_set_.end()) {
        return it->second.second;
    }
    return std::monostate{};
}

bool session_variable_set::set(std::string_view name, value_type value) {
    auto it = variable_set_.find(std::string(name));
    if (it != variable_set_.end()) {
        it->second.second = std::move(value);
        return true;
    }
    return false;
}

bool session_variable_set::unset(std::string_view name) {
    auto it = variable_set_.find(std::string(name));
    if (it != variable_set_.end()) {
        variable_set_.erase(it);
        return true;
    }
    return false;
}

}
