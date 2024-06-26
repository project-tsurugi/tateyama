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

#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <tateyama/logging.h>
#include <glog/logging.h>

#include <tateyama/status/resource/bridge.h>
#include "database_info_impl.h"

namespace tateyama::status_info {   // FIXME should be tateyama::status::resource

// resource_status_memory
void resource_status_memory::set_pid() {
    resource_status_->pid_ = ::getpid();
}
void resource_status_memory::whole(state s) {
    resource_status_->whole_ = s;
}
void resource_status_memory::mutex_file(std::string_view file) {
    resource_status_->mutex_file_ = file;
}
std::string_view resource_status_memory::mutex_file() const {
    auto& mutex_file = resource_status_->mutex_file_;
    return {mutex_file.data(), mutex_file.size()};
}
void resource_status_memory::set_database_name(std::string_view name) {
    resource_status_->database_name_ = name;
}
std::string_view resource_status_memory::get_database_name() {
    return resource_status_->database_name_;
}

void resource_status_memory::set_maximum_sessions(std::size_t n) {
    auto& sessions = resource_status_->sessions_;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        sessions.resize(n * mergin);
        for (std::size_t& session : sessions) {
            session = inactive_session_id;
        }
    }
}

static inline std::size_t index(std::size_t n) {
    return n * resource_status_memory::mergin;
}
void resource_status_memory::add_shm_entry(std::size_t session_id, std::size_t slot) {
    auto& sessions = resource_status_->sessions_;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        for (std::size_t i = 0; i < mergin; i++) {
            auto idx = index(slot) + i;
            if (sessions.at(idx) == inactive_session_id) {
                sessions.at(idx) = session_id;
                return;
            }
        }
        std::stringstream ss{};
        ss << "IPC communication channel for session ";
        for (std::size_t i = 0; i < mergin; i++) {
            auto idx = index(slot) + i;
            ss << sessions.at(idx) << ' ';
        }
        ss<< "remains in slot " << slot << ", thus cannot create that for session " << session_id;
        throw std::runtime_error(ss.str());
    }
}
void resource_status_memory::remove_shm_entry(std::size_t session_id, std::size_t slot) {
    auto& sessions = resource_status_->sessions_;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        for (std::size_t i = 0; i < mergin; i++) {
            auto idx = index(slot) + i;
            if (sessions.at(idx) == session_id) {
                sessions.at(idx) = inactive_session_id;
                return;
            }
        }
    }
}

}
