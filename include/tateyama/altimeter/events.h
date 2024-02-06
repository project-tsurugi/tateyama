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

//
// The following uses the same style as altimeter/logger/examples/altimeter/main.cpp
//
namespace tateyama::altimeter {
// log category list
namespace log_category {
static constexpr std::string_view event = "event";
static constexpr std::string_view audit = "audit";
} // namespace log_category

// event log type list
namespace log_type::event {
static constexpr std::string_view session_start = "session_start";
static constexpr std::string_view session_end = "session_end";
} // namespace log_type::event

// event log item list
namespace log_item::event {
static constexpr std::string_view user = "user";
static constexpr std::string_view dbname = "dbname";
static constexpr std::string_view pid = "pid";
static constexpr std::string_view remote_host = "remote_host";
static constexpr std::string_view application_name = "application_name";
static constexpr std::string_view session_id = "session_id";
static constexpr std::string_view session_label = "session_label";
}

// event log level list
namespace log_level::event {
static constexpr int error = 30;
static constexpr int info = 50;
} // namespace log_level::event

// audit log type list
namespace log_type::audit {
static constexpr std::string_view db_start = "db_start";
static constexpr std::string_view db_stop = "db_stop";
} // namespace log_type::audit

// audit log item list
namespace log_item::audit {
static constexpr std::string_view user = "user";
static constexpr std::string_view dbname = "dbname";
static constexpr std::string_view result = "result";
}

// audit log level list
namespace log_level::audit {
static constexpr int error = 30;
static constexpr int info = 50;
} // namespace log_level::audit

}  // tateyama::altimeter
