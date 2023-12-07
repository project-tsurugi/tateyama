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

#include <chrono>

namespace tateyama::api::server {

/**
 * @brief session_info
 */
class database_info {
public:
    /**
     * @brief the process ID type.
     */
    using process_id_type = pid_t;

    /**
     * @brief the time point type.
     */
    using time_type = std::chrono::time_point<std::chrono::system_clock>;

    /**
     * @brief returns the process ID of the current database process.
     * @return the database process ID
     */
    virtual process_id_type process_id() const noexcept = 0;

    /**
     * @brief the current database name.
     * @return the database name
     */
    virtual std::string_view name() const noexcept = 0;

    /**
     * @brief returns the time point when the current database process was started.
     * @returns the database started time
     */
    virtual time_type start_at() const noexcept = 0;
};

}
