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
#include <vector>
#include <functional>

#include <tateyama/session/context.h>

namespace tateyama::session {

/**
 * @brief provides living database sessions.
 */
class session_container {
public:
    /**
     * @brief construct the object
     */
    session_container() = default;

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

    session_container(session_container const&) = delete;
    session_container(session_container&&) = delete;
    session_container& operator = (session_container const&) = delete;
    session_container& operator = (session_container&&) = delete;

protected:
    ~session_container() = default;

private:
    /**
     * @brief apply func to all entries stored in session_contexts_ with doing garbage collection
     * @param func the function that takes std::shared_ptr<session_context>> as argument
     */
    virtual void foreach(const std::function<void(const std::shared_ptr<session_context>&)>& func) = 0;

    // ...

};

}
