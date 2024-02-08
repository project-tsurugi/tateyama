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

#include <tateyama/session/resource/variable_declaration_set.h>

namespace tateyama::session::resource {

session_variable_declaration_set::session_variable_declaration_set() noexcept = default;

session_variable_declaration_set::session_variable_declaration_set(std::vector<session_variable_declaration> declarations) noexcept :
    declarations_(declarations) {
}

bool session_variable_declaration_set::declare(session_variable_declaration declaration) {
    if (find(declaration.name())) {
        return false;
    }
    declarations_.emplace_back(std::move(declaration));
    return true;
}

session_variable_declaration const* session_variable_declaration_set::find(std::string_view name) const noexcept {
    for (const auto& e : declarations_) {
        if (e.name() == name) {
            return &e;
        }
    }
    return nullptr;
}

std::vector<std::string> session_variable_declaration_set::enumerate_variable_names() const {
    std::vector<std::string> rv{};
    
    for (const auto& e : declarations_) {
        rv.emplace_back(e.name());
    }
    return rv;
}

session_variable_set session_variable_declaration_set::make_variable_set() const {
    std::vector<std::tuple<std::string, session_variable_set::variable_type, session_variable_set::value_type>> declarations{};

    for (const auto& e : declarations_) {
        declarations.emplace_back(e.name(), e.type(), e.initial_value_);
    }
    return session_variable_set(declarations);
}

}
