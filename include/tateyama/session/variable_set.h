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

#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <map>

#include <tateyama/session/variable_type.h>

namespace tateyama::session {

/**
 * @brief a set of session variables.
 * @see session_variable_declaration_set
 */
class session_variable_set {
public:
    /**
     * @brief the variable type.
     */
    using variable_type = session_variable_type;

    /**
     * @brief the value type.
     * @details if the value indicates std::monostate, it represents "the value is not set."
     */
    using value_type = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, std::string>;

    /**
     * @brief creates a new empty instance.
     */
    session_variable_set() noexcept;

    /**
     * @brief creates a new instance.
     * @param declarations the variable declarations that consists of (variable name, variable type, initial value).
     */
    explicit session_variable_set(std::vector<std::tuple<std::string, variable_type, value_type>> declarations);

    /**
     * @brief returns whether or not the variable with the specified name exists.
     * @param name the variable name
     * @return true the variable with the name exists
     * @return false there are no variables with the name
     */
    [[nodiscard]] bool exists(std::string_view name) const noexcept;

    /**
     * @brief returns the type of the variable with the specified name.
     * @param name the variable name
     * @return the type of the target variable
     * @return empty if such the variable is not declared
     */
    [[nodiscard]] std::optional<variable_type> type(std::string_view name) const noexcept;

    /**
     * @brief return the value of the variable with the specified name.
     * @param name the variable name
     * @return the value in the target variable if it is set
     * @return std::monostate if such the variable is not declared, or the value is not set
     */
    [[nodiscard]] value_type get(std::string_view name) const;

    /**
     * @brief sets the value to the variable with the specified name.
     * @param name the variable name
     * @param value the value to set
     * @return true if successfully set the value
     * @return false if the target variable is not declared, or the value type is not suitable for the variable
     * @see type()
     */
    bool set(std::string_view name, value_type value);

    // ...

private:
    std::map<std::string, std::pair<variable_type, value_type>> variable_set_{};
};

}
