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

#include <tateyama/session/variable_declaration_set.h>

#include "container_impl.h"
#include <tateyama/session/core.h>

namespace tateyama::session::resource {

class bridge;

/**
 * @brief the core class of `sessions` resource that provides information about living sessions.
 */
class sessions_core_impl : public tateyama::session::sessions_core {
public:
    /**
     * @brief construct the object
     */
    sessions_core_impl() = default;
    
    /**
     * @brief destruct the object
     */
    virtual ~sessions_core_impl() = default;
    
    /**
     * @brief returns the session container.
     * @return the session container
     */
    session_container& sessions() noexcept override;

    /// @copydoc sessions()
    [[nodiscard]] session_container const& sessions() const noexcept override;

    /**
     * @brief returns the session variable declarations.
     * @return the session variable declarations.
     */
    session_variable_declaration_set& variable_declarations() noexcept override;

    /// @copydoc variable_declarations()
    [[nodiscard]] session_variable_declaration_set const& variable_declarations() const noexcept override;

    // ...

    sessions_core_impl(sessions_core_impl const&) = delete;
    sessions_core_impl(sessions_core_impl&&) = delete;
    sessions_core_impl& operator = (sessions_core_impl const&) = delete;
    sessions_core_impl& operator = (sessions_core_impl&&) = delete;
    
private:
    session_container_impl container_{};

    session_variable_declaration_set variable_declarations_{};

    friend class bridge;
};

}
