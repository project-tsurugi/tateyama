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
    explicit database_info_impl(std::string_view name, std::string_view instance_id) : name_(name), instance_id_(instance_id) {}

    database_info_impl() : database_info_impl("name_has_not_been_given", "instance_id_has_not_been_given") {}

    [[nodiscard]] process_id_type process_id() const noexcept override { return process_id_; }

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    [[nodiscard]] time_type start_at() const noexcept override { return start_at_; }

    [[nodiscard]] std::string_view instance_id() const noexcept override { return instance_id_; }

private:
    std::string name_;

    std::string instance_id_;

    process_id_type process_id_{::getpid()};

    time_type start_at_{std::chrono::system_clock::now()};
};

}
