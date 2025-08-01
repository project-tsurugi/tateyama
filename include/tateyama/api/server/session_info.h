/*
 * Copyright 2018-2025 Project Tsurugi.
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
#include <chrono>

namespace tateyama::api::server {

/**
 * @brief represents the login user type.
 */
enum class user_type {
    /**
     * @brief administrators.
     * @details administrators have access to all resources without authorization restrictions.
     */
    administrator,

    /**
     * @brief standard users.
     * @details standard users are restricted to only authorized operations on their respective resources.
     */
    standard,
};

/**
 * @brief session_info
 */
class session_info {
public:
    /**
     * @brief the numeric session ID type.
     */
    using id_type = std::uint64_t;

    /**
     * @brief the time point type.
     */
    using time_type = std::chrono::time_point<std::chrono::system_clock>;

    /**
     * @brief create empty object
     */
    session_info() = default;

    /**
     * @brief destruct the object
     */
    virtual ~session_info() = default;

    session_info(session_info const& other) = default;
    session_info& operator=(session_info const& other) = default;
    session_info(session_info&& other) noexcept = default;
    session_info& operator=(session_info&& other) noexcept = default;

    /**
     * @brief returns the current numeric session ID.
     * @returns the current numeric session ID
     * @note when you denote session IDs, it MUST start with `:` (colon), and trim leading zeros.
     *    For example, `1234` must be `:1234`.
     */
    [[nodiscard]] virtual id_type id() const noexcept = 0;

    /**
     * @brief returns the current session label.
     * @returns the session label
     * @returns empty if the session label is not defined
     * @note session labels MUST NOT start with `:` (colon).
     * @note session labels may not unique to other sessions.
     */
    [[nodiscard]] virtual std::string_view label() const noexcept = 0;

    /**
     * @brief returns the application name which started this session.
     * @returns the application name
     * @returns empty if the application name is not sure
     */
    [[nodiscard]] virtual std::string_view application_name() const noexcept = 0;

    /**
     * @brief returns the time point when the current session was started.
     * @returns the session started time
     */
    [[nodiscard]] virtual time_type start_at() const noexcept = 0;

    /**
     * @brief returns the connection type name (e.g. `ipc`).
     * @returns the connection type name
     * @note This name is defined for each endpoint to which the session connects to,
     *    and may equal to such the endpoint name.
     */
    [[nodiscard]] virtual std::string_view connection_type_name() const noexcept = 0;

    /**
     * @brief returns the connection information information.
     * @details This string can be used to determine where the session is connected from,
     *    and its contents differ for individual endpoint types.
     *    For example, IP address and port number is listed for the TCP endpoint.
     * @returns the connection information
     */
    [[nodiscard]] virtual std::string_view connection_information() const noexcept = 0;

    /**
     * @brief returns the authenticated user name.
     * @return the authenticated user name
     * @return std::nullopt if authentication is not enabled
     */
    [[nodiscard]] virtual std::optional<std::string_view> username() const noexcept = 0;

    /**
     * @brief returns the user type.
     * @return the login user type
     * @return user_type::administrator if authentication is not enabled
     */
    [[nodiscard]] virtual tateyama::api::server::user_type user_type() const noexcept = 0;
};

}
