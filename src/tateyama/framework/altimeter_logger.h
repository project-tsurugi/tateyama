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
#include <sys/types.h>
#include <unistd.h>

#include <altimeter/configuration.h>
#include <altimeter/log_item.h>
#include <altimeter/logger.h>

#include <tateyama/altimeter/events.h>

namespace tateyama::framework {

using namespace tateyama::altimeter;
    
static constexpr std::int64_t db_start_stop_success = 1;
static constexpr std::int64_t db_start_stop_fail = 2;

static inline void db_start(std::string_view user, std::string_view dbname, std::int64_t result) {
    if (::altimeter::logger::is_log_on(log_category::audit,
                                     log_level::audit::info)) {
        ::altimeter::log_item log_item;
        log_item.category(log_category::audit);
        log_item.type(log_type::audit::db_start);
        log_item.level(log_level::audit::info);
        if (!user.empty()) {
            log_item.add(log_item::audit::user, user);
        }
        if (!dbname.empty()) {
            log_item.add(log_item::audit::dbname, dbname);
        }
        log_item.add(log_item::audit::result, result);
        ::altimeter::logger::log(log_item);
    }
}

static inline void db_stop(std::string_view user, std::string_view dbname, std::int64_t result, std::int64_t duration_time) {
    if (::altimeter::logger::is_log_on(log_category::audit,
                                     log_level::audit::info)) {
        ::altimeter::log_item log_item;
        log_item.category(log_category::audit);
        log_item.type(log_type::audit::db_stop);
        log_item.level(log_level::audit::info);
        if (!user.empty()) {
            log_item.add(log_item::audit::user, user);
        }
        if (!dbname.empty()) {
            log_item.add(log_item::audit::dbname, dbname);
        }
        log_item.add(log_item::audit::result, result);
        log_item.add(log_item::audit::duration_time, duration_time);
        ::altimeter::logger::log(log_item);
    }
}

}  // tateyama::endpoint::altimeter
