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

#include <string_view>
#include <atomic>

#include <tateyama/api/server/session_info.h>
#include <tateyama/session/resource/variable_set.h>
#include <tateyama/session/resource/shutdown_request_type.h>

namespace tateyama::session::resource {

using session_info = tateyama::api::server::session_info;

/**
 * @brief Provides living database sessions.
 */
class session_context {
public:
    /**
     * @brief the numeric session ID type.
     */
    using numeric_id_type = std::size_t;

    /**
     * @brief creates a new instance.
     * @param info information about this session
     * @param variables the set of session variable
     */
    session_context(
        session_info& info,
        session_variable_set variables) noexcept;

    /**
     * @brief returns the numeric ID of this session.
     * @return the numeric session ID
     */
    [[nodiscard]] numeric_id_type numeric_id() const noexcept;

    /**
     * @brief returns the symbolic ID of this session.
     * @return the symbolic session ID
     * @return empty if the symbolic ID is not defined
     */
    [[nodiscard]] std::string_view symbolic_id() const noexcept;

    /**
     * @brief returns the session information.
     * @return the session information
     */
    [[nodiscard]] session_info const& info() const noexcept;

    /**
     * @brief returns the session variable set.
     * @return the session variable set
     */
    session_variable_set& variables() noexcept;

    /// @copydoc variables()
    [[nodiscard]] session_variable_set const& variables() const noexcept;

    /**
     * @brief returns the shutdown request status for this session.
     * @return the shutdown request status for this session
     */
    [[nodiscard]] shutdown_request_type shutdown_request() const noexcept;

    /**
     * @brief requests to shutdown this session.
     * @param status the request type
     * @return true if request was accepted, or already requested the specified request type
     * @return false if request was rejected by conflict to other request
     */
    bool shutdown_request(shutdown_request_type type) noexcept;

    // ...

private:
    session_info& info_;
    session_variable_set variables_;
    std::atomic<shutdown_request_type> shutdown_request_{shutdown_request_type::nothing};
};

}
