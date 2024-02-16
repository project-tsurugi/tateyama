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
#pragma once

#include <string>

#include <tateyama/session/resource/variable_set.h>
#include <tateyama/session/resource/variable_type.h>

namespace tateyama::session::resource {

class session_variable_declaration_set;

/**
 * @brief represents declaration of a session variable.
 * @see session_variable_declaration_set
 */
class session_variable_declaration {
public:
    /**
     * @brief creates a new instance.
     * @param name the variable name
     * @param type the variable value type
     * @param initial_value the variable initial value, must be a suitable value type for the variable
     * @param description the description of this session variable
     */
    session_variable_declaration(
            std::string name,
            session_variable_type type,
            session_variable_set::value_type initial_value = {},
            std::string description = {}) noexcept;

    /**
     * @brief returns the variable name.
     * @return the variable name
     */
    [[nodiscard]] std::string const& name() const noexcept;

    /**
     * @brief returns the variable value type.
     * @return the value type
     */
    [[nodiscard]] session_variable_type type() const noexcept;

    /**
     * @brief returns the description of this session variable.
     * @return the session variable
     */
    [[nodiscard]] std::string const& description() const noexcept;

    // ...

private:
    std::string name_;
    session_variable_type type_;
    session_variable_set::value_type initial_value_;
    std::string description_;

    friend class session_variable_declaration_set;
};

}
