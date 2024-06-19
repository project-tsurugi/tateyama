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

#include <sys/types.h>
#include <unistd.h>

#include <altimeter/configuration.h>
#include <altimeter/log_item.h>
#include <altimeter/logger.h>

#include <altimeter/event/constants.h>
#include <altimeter/audit/constants.h>

#include <tateyama/api/server/database_info.h>
#include <tateyama/api/server/session_info.h>

namespace tateyama::endpoint::altimeter {

static inline void session_start(const tateyama::api::server::database_info& database_info, const tateyama::api::server::session_info& session_info) {
    if (::altimeter::logger::is_log_on(::altimeter::event::category,
                                       ::altimeter::event::level::session)) {
        ::altimeter::log_item log_item;
        log_item.category(::altimeter::event::category);
        log_item.type(::altimeter::event::type::session_start);
        log_item.level(::altimeter::event::level::session);
        if (auto database_name = database_info.name(); !database_name.empty()) {
            log_item.add(::altimeter::event::item::dbname, database_name);
        }
        log_item.add(::altimeter::event::item::pid, static_cast<pid_t>(database_info.process_id()));
        if (auto connection_information = session_info.connection_information(); !connection_information.empty()) {
            log_item.add(::altimeter::event::item::remote_host, connection_information);
        }
        if (auto application_name = session_info.application_name(); !application_name.empty()) {
            log_item.add(::altimeter::event::item::application_name, application_name);
        }
        if (auto session_label = session_info.label(); !session_label.empty()) {
            log_item.add(::altimeter::event::item::session_label, session_label);
        }
        log_item.add(::altimeter::event::item::session_id, static_cast<std::int64_t>(session_info.id()));
        ::altimeter::logger::log(log_item);
    }
}

static inline void session_end(const tateyama::api::server::database_info& database_info, const tateyama::api::server::session_info& session_info, std::int64_t duration_time) {
    if (::altimeter::logger::is_log_on(::altimeter::event::category,
                                       ::altimeter::event::level::session)) {
        ::altimeter::log_item log_item;
        log_item.category(::altimeter::event::category);
        log_item.type(::altimeter::event::type::session_end);
        log_item.level(::altimeter::event::level::session);
        if (auto database_name = database_info.name(); !database_name.empty()) {
            log_item.add(::altimeter::event::item::dbname, database_name);
        }
        log_item.add(::altimeter::event::item::pid, static_cast<pid_t>(database_info.process_id()));
        if (auto connection_information = session_info.connection_information(); !connection_information.empty()) {
            log_item.add(::altimeter::event::item::remote_host, connection_information);
        }
        if (auto application_name = session_info.application_name(); !application_name.empty()) {
            log_item.add(::altimeter::event::item::application_name, application_name);
        }
        if (auto session_label = session_info.label(); !session_label.empty()) {
            log_item.add(::altimeter::event::item::session_label, session_label);
        }
        log_item.add(::altimeter::event::item::session_id, static_cast<std::int64_t>(session_info.id()));
        log_item.add(::altimeter::event::item::duration_time, duration_time);
        ::altimeter::logger::log(log_item);
    }
}

}  // tateyama::endpoint::altimeter
