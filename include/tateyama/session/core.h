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

#include <tateyama/session/container.h>
#include <tateyama/session/variable_declaration_set.h>


namespace tateyama::session {

/**
 * @brief the core class of `sessions` resource that provides information about living sessions.
 */
class sessions_core {
public:
    /**
     * @brief returns the session container.
     * @return the session container
     */
    virtual session_container& sessions() noexcept = 0;

    /// @copydoc sessions()
    [[nodiscard]] virtual session_container const& sessions() const noexcept = 0;

    /**
     * @brief returns the session variable declarations.
     * @return the session variable declarations.
     */
    virtual session_variable_declaration_set& variable_declarations() noexcept = 0;

    /// @copydoc variable_declarations()
    [[nodiscard]] virtual session_variable_declaration_set const& variable_declarations() const noexcept = 0;

    // ...

    sessions_core(sessions_core const&) = delete;
    sessions_core(sessions_core&&) = delete;
    sessions_core& operator = (sessions_core const&) = delete;
    sessions_core& operator = (sessions_core&&) = delete;

protected:
    sessions_core() = default;
    ~sessions_core() = default;
};

}
