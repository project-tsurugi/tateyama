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
#include <string_view>

#include <tateyama/session/resource/variable_set.h>
#include <tateyama/session/resource/variable_declaration.h>

namespace tateyama::session::resource {

/**
 * @brief declarations of session variables.
 * @details This can declare session variables used in session_variable_set.
 * @see session_variable_set
 */
class session_variable_declaration_set {
public:
    /**
     * @brief creates a new empty instance.
     */
    session_variable_declaration_set() noexcept;

    /**
     * @brief creates a new instance with initial variable declarations.
     * @param declarations the initial list of the variable declarations
     * @attention undefined behavior if some declarations has the same name
     */
    explicit session_variable_declaration_set(std::vector<session_variable_declaration> declarations) noexcept;

    /**
     * @brief declares a new session variable.
     * @param declaration the session variable to declare
     * @return true if the variable is successfully declared
     * @return false if the variable is already declared in this set
     */
    bool declare(session_variable_declaration declaration);

    /**
     * @brief returns a previously declared session variable.
     * @param name the variable name
     * @return a pointer to the variable if it declared
     * @return nullptr if there are no such the variable with the specified name
     */
    [[nodiscard]] session_variable_declaration const* find(std::string_view name) const noexcept;

    /**
     * @brief return the previously declared session variable names.
     * @return the list of declared variable names
     */
    [[nodiscard]] std::vector<std::string> enumerate_variable_names() const;

    /**
     * @brief creates a new session_variable_set that holds variables declared in this object.
     * @return a new session variable set previously declared in this object
     */
    [[nodiscard]] session_variable_set make_variable_set() const;

    // ...

private:
    std::vector<session_variable_declaration> declarations_{};
};

}
