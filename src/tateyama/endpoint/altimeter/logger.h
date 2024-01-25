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

#include <tateyama/api/server/database_info.h>
#include <tateyama/api/server/session_info.h>
#include <tateyama/altimeter/events.h>

namespace tateyama::endpoint::altimeter {
//
// The following two methods are created with reference to altimeter/logger/examples/altimeter/main.cpp
//

using namespace tateyama::altimeter;
    
static inline void session_start(const tateyama::api::server::database_info& database_info, const tateyama::api::server::session_info& session_info) {
    if (::altimeter::logger::is_log_on(log_category::event,
                                     log_level::event::info)) {
        ::altimeter::log_item log_item;
        log_item.category(log_category::event);
        log_item.type(log_type::event::session_start);
        log_item.level(log_level::event::info);
        if (auto database_name = database_info.name(); !database_name.empty()) {
            log_item.add(log_item::event::dbname, database_name);
        }
        log_item.add(log_item::event::pid, static_cast<pid_t>(database_info.process_id()));
        if (auto connection_information = session_info.connection_information(); !connection_information.empty()) {
            log_item.add(log_item::event::remote_host, connection_information);
        }
        if (auto application_name = session_info.application_name(); !application_name.empty()) {
            log_item.add(log_item::event::application_name, application_name);
        }
        if (auto session_label = session_info.label(); !session_label.empty()) {
            log_item.add(log_item::event::session_label, session_label);
        }
        log_item.add(log_item::event::session_id, static_cast<std::int64_t>(session_info.id()));
        ::altimeter::logger::log(log_item);
    }
}

static inline void session_end(const tateyama::api::server::database_info& database_info, const tateyama::api::server::session_info& session_info) {
    if (::altimeter::logger::is_log_on(log_category::event,
                                     log_level::event::info)) {
        ::altimeter::log_item log_item;
        log_item.category(log_category::event);
        log_item.type(log_type::event::session_end);
        log_item.level(log_level::event::info);
        if (auto database_name = database_info.name(); !database_name.empty()) {
            log_item.add(log_item::event::dbname, database_name);
        }
        log_item.add(log_item::event::pid, static_cast<pid_t>(database_info.process_id()));
        if (auto connection_information = session_info.connection_information(); !connection_information.empty()) {
            log_item.add(log_item::event::remote_host, connection_information);
        }
        if (auto application_name = session_info.application_name(); !application_name.empty()) {
            log_item.add(log_item::event::application_name, application_name);
        }
        if (auto session_label = session_info.label(); !session_label.empty()) {
            log_item.add(log_item::event::session_label, session_label);
        }
        log_item.add(log_item::event::session_id, static_cast<std::int64_t>(session_info.id()));
        ::altimeter::logger::log(log_item);
    }
}

}  // tateyama::endpoint::altimeter
