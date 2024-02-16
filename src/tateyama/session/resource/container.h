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

#include <memory>
#include <map>
#include <vector>

#include <tateyama/session/resource/context.h>

namespace tateyama::session::resource {

/**
 * @brief provides living database sessions.
 */
class session_container {
public:
    /**
     * @brief registers a new session context.
     * @param session the session context to register
     * @return true if the target session is successfully registered
     * @return false if the target session is not registered
     *    because another session with such the numeric ID already exists in this container
     * @note Symbolic session ID may duplicate in this container
     */
    bool register_session(std::shared_ptr<session_context> const& session);

    /**
     * @brief find for a session context with such the numeric ID.
     * @param numeric_id the numeric ID of the target session
     * @return the corresponded session context
     * @return empty if there is no such the session
     */
    [[nodiscard]] std::shared_ptr<session_context> find_session(session_context::numeric_id_type numeric_id) const;

    /**
     * @brief enumerates the list of numeric IDs of sessions in this container.
     * @return the list of numeric IDs
     * @return empty if there are no sessions in this container
     */
    [[nodiscard]] std::vector<session_context::numeric_id_type> enumerate_numeric_ids() const;

    /**
     * @brief enumerates the list of numeric IDs of sessions with the specified symbolic ID.
     * @param symbolic_id the symbolic ID of the target sessions
     * @return the list of numeric IDs
     * @return empty if there are no sessions with such the symbolic ID
     */
    [[nodiscard]] std::vector<session_context::numeric_id_type> enumerate_numeric_ids(std::string_view symbolic_id) const;

    // ...

private:
    std::map<session_context::numeric_id_type, std::shared_ptr<session_context>> session_contexts_{};

    std::vector<session_context::numeric_id_type> numeric_ids_{};

    friend class bridge;
};

}
