/*
 * Copyright 2024-2024 Project Tsurugi.
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

namespace tateyama::datastore::service {

using namespace tateyama::altimeter;

static constexpr std::int64_t backup_restore_success = 1;
static constexpr std::int64_t backup_restore_fail = 2;

static inline void backup(const std::shared_ptr<request>& req, std::string_view command, std::int64_t result) {
    if (::altimeter::logger::is_log_on(log_category::audit, log_level::audit::info)) {
        ::altimeter::log_item log_item;
        log_item.category(log_category::audit);
        log_item.type(log_type::audit::backup);
        log_item.level(log_level::audit::info);
        if (auto user = req->session_info().user_name(); !user.empty()) {
            log_item.add(log_item::audit::user, user);
        }
        if (auto dbname = req->database_info().name(); !dbname.empty()) {
            log_item.add(log_item::audit::dbname, dbname);
        }
        if (auto connection_information = req->session_info().connection_information(); !connection_information.empty()) {
            log_item.add(log_item::event::remote_host, connection_information);
        }
        if (auto application_name = req->session_info().application_name(); !application_name.empty()) {
            log_item.add(log_item::event::application_name, application_name);
        }
        log_item.add(log_item::audit::result, result);
        if (!command.empty()) {
            log_item.add(log_item::audit::command, command);
        }
        ::altimeter::logger::log(log_item);
    }
}

static inline void restore(const std::shared_ptr<request>& req, std::string_view command, std::int64_t result) {
    if (::altimeter::logger::is_log_on(log_category::audit, log_level::audit::info)) {
        ::altimeter::log_item log_item;
        log_item.category(log_category::audit);
        log_item.type(log_type::audit::restore);
        log_item.level(log_level::audit::info);
        if (auto user = req->session_info().user_name(); !user.empty()) {
            log_item.add(log_item::audit::user, user);
        }
        if (auto dbname = req->database_info().name(); !dbname.empty()) {
            log_item.add(log_item::audit::dbname, dbname);
        }
        if (auto connection_information = req->session_info().connection_information(); !connection_information.empty()) {
            log_item.add(log_item::event::remote_host, connection_information);
        }
        if (auto application_name = req->session_info().application_name(); !application_name.empty()) {
            log_item.add(log_item::event::application_name, application_name);
        }
        log_item.add(log_item::audit::result, result);
        if (!command.empty()) {
            log_item.add(log_item::audit::command, command);
        }
        ::altimeter::logger::log(log_item);
    }
}

}
