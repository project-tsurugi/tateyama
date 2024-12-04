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
#pragma once

#include <string_view>
#include <memory>

#include <tateyama/session/variable_set.h>
#include "database_info.h"
#include "session_info.h"
#include "session_store.h"

namespace tateyama::api::server {

/**
 * @brief request interface
 */
class request {
public:
    /**
     * @brief create empty object
     */
    request() = default;

    /**
     * @brief destruct the object
     */
    virtual ~request() = default;

    request(request const& other) = default;
    request& operator=(request const& other) = default;
    request(request&& other) noexcept = default;
    request& operator=(request&& other) noexcept = default;

    /**
     * @brief accessor to session identifier
     */
    [[nodiscard]] virtual std::size_t session_id() const = 0;

    /**
     * @brief accessor to target service identifier
     */
    [[nodiscard]] virtual std::size_t service_id() const = 0;

    /**
     * @brief accessor to local identifier
     */
    [[nodiscard]] virtual std::size_t local_id() const = 0;

    /**
     * @brief accessor to the payload binary data
     * @return the view to the payload
     */
    [[nodiscard]] virtual std::string_view payload() const = 0;  //NOLINT(modernize-use-nodiscard)

    /**
     * @brief returns the current database information.
     * @returns the current database information
     */
    [[nodiscard]] virtual tateyama::api::server::database_info const& database_info() const noexcept = 0;

    /**
     * @brief returns the current session information.
     * @returns the current session information
     */
    [[nodiscard]] virtual tateyama::api::server::session_info const& session_info() const noexcept = 0;

    /**
     * @brief returns the current session store.
     * @returns the current session store
     */
    [[nodiscard]] virtual tateyama::api::server::session_store& session_store() noexcept = 0;

    /**
     * @brief returns the current session session variable set.
     * @returns the current session variable set
     */
    [[nodiscard]] virtual tateyama::session::session_variable_set& session_variable_set() noexcept = 0;
};

}
