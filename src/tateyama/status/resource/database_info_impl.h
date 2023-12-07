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

#include <sys/types.h>
#include <unistd.h>

#include <tateyama/api/server/database_info.h>

namespace tateyama::status_info::resource {

class bridge;

/**
 * @brief database_info_impl
 */
class database_info_impl : public tateyama::api::server::database_info {
public:
    explicit database_info_impl(std::string_view name) : name_(name) {}

    database_info_impl() : database_info_impl("name_has_not_been_given") {}
    
    process_id_type process_id() const noexcept { return process_id_; }

    std::string_view name() const noexcept { return name_; }

    time_type start_at() const noexcept { return start_at_; }

private:
    process_id_type process_id_{::getpid()};

    std::string name_{};

    time_type start_at_{std::chrono::system_clock::now()};

    // only tateyama::status_info::resource::bridge can use this.
    void name(std::string_view name) { name_ = name; }

    friend class tateyama::status_info::resource::bridge;
};

}
