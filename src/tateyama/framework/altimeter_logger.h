/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <altimeter/event/constants.h>
#include <altimeter/audit/constants.h>

namespace tateyama::framework {

static constexpr std::int64_t db_start_stop_success = 1;
static constexpr std::int64_t db_start_stop_fail = 2;

static inline void db_start(std::string_view user, std::string_view dbname, std::string_view instance_id, std::int64_t result) {
    if (::altimeter::logger::is_log_on(::altimeter::audit::category,
                                       ::altimeter::audit::level::info)) {
        ::altimeter::log_item log_item;
        log_item.category(::altimeter::audit::category);
        log_item.type(::altimeter::audit::type::db_start);
        log_item.level(::altimeter::audit::level::info);
        if (!user.empty()) {
            log_item.add(::altimeter::audit::item::user, user);
        }
        if (!dbname.empty()) {
            log_item.add(::altimeter::audit::item::dbname, dbname);
        }
        log_item.add(::altimeter::audit::item::instance_id, instance_id);
        log_item.add(::altimeter::audit::item::result, result);
        ::altimeter::logger::log(log_item);
    }
    if (::altimeter::logger::is_log_on(::altimeter::event::category,
                                       ::altimeter::event::level::database)) {
        ::altimeter::log_item log_item;
        log_item.category(::altimeter::event::category);
        log_item.type(::altimeter::event::type::db_start);
        log_item.level(::altimeter::event::level::database);
        if (!user.empty()) {
            log_item.add(::altimeter::event::item::user, user);
        }
        if (!dbname.empty()) {
            log_item.add(::altimeter::event::item::dbname, dbname);
        }
        log_item.add(::altimeter::event::item::pid, getpid());
        log_item.add(::altimeter::event::item::current_level,
                     ::altimeter::logger::get_level(::altimeter::event::category));
        log_item.add(::altimeter::event::item::instance_id, instance_id);
        log_item.add(::altimeter::event::item::result, result);
        ::altimeter::logger::log(log_item);
    }
}

static inline void db_stop(std::string_view user, std::string_view dbname, std::string_view instance_id, std::int64_t result, std::int64_t duration_time) {
    if (::altimeter::logger::is_log_on(::altimeter::audit::category,
                                       ::altimeter::audit::level::info)) {
        ::altimeter::log_item log_item;
        log_item.category(::altimeter::audit::category);
        log_item.type(::altimeter::audit::type::db_stop);
        log_item.level(::altimeter::audit::level::info);
        if (!user.empty()) {
            log_item.add(::altimeter::audit::item::user, user);
        }
        if (!dbname.empty()) {
            log_item.add(::altimeter::audit::item::dbname, dbname);
        }
        log_item.add(::altimeter::audit::item::instance_id, instance_id);
        log_item.add(::altimeter::audit::item::result, result);
        // no duration_time for audit log (by sepcification)
        ::altimeter::logger::log(log_item);
    }
    if (::altimeter::logger::is_log_on(::altimeter::event::category,
                                       ::altimeter::event::level::database)) {
        ::altimeter::log_item log_item;
        log_item.category(::altimeter::event::category);
        log_item.type(::altimeter::event::type::db_stop);
        log_item.level(::altimeter::event::level::database);
        if (!user.empty()) {
            log_item.add(::altimeter::event::item::user, user);
        }
        if (!dbname.empty()) {
            log_item.add(::altimeter::event::item::dbname, dbname);
        }
        log_item.add(::altimeter::event::item::pid, getpid());
        log_item.add(::altimeter::event::item::instance_id, instance_id);
        log_item.add(::altimeter::event::item::result, result);
        log_item.add(::altimeter::event::item::duration_time, duration_time);
        ::altimeter::logger::log(log_item);
    }
}

}  // tateyama::endpoint::altimeter
