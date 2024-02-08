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

#include "tateyama/session/resource/variable_declaration.h"

namespace tateyama::session::resource {

session_variable_declaration::session_variable_declaration(
    std::string name,
    session_variable_type type,
    session_variable_set::value_type initial_value,
    std::string description) noexcept
    : name_(name), type_(type), initial_value_(initial_value), description_(description) {
}

std::string const& session_variable_declaration::name() const noexcept {
    return name_;
}

session_variable_type session_variable_declaration::type() const noexcept {
    return type_;
}

std::string const& session_variable_declaration::description() const noexcept {
    return description_;
}

}
