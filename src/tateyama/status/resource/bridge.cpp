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

#include <iomanip>
#include <sstream>

#include "tateyama/status/resource/bridge.h"

namespace tateyama::status_info::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment& env) {
    auto conf = env.configuration();
    set_digest(conf->get_canonical_path().string());

    auto database_name_opt = conf->get_section("ipc_endpoint")->get<std::string>("database_name");
    if (!database_name_opt) {
        LOG(ERROR) << "cannot find database_name at the section in the configuration";
        return false;
    }
    auto name = database_name_opt.value();
    database_info_.name(name);

    std::string status_file_name{file_prefix};
    status_file_name += digest_;
    status_file_name += ".stat";
    boost::interprocess::shared_memory_object::remove(status_file_name.c_str());
    shm_remover_ = std::make_unique<shm_remover>(status_file_name);
    try {
        segment_ = std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, status_file_name.c_str(), shm_size);
        resource_status_memory_ = std::make_unique<resource_status_memory>(*segment_);
        resource_status_memory_->set_pid();
        resource_status_memory_->set_database_name(name);
        return true;
    } catch(const boost::interprocess::interprocess_exception& ex) {
        return false;
    }
}

bool bridge::start([[maybe_unused]] environment& env) {
    return true;
}

bool bridge::shutdown([[maybe_unused]] environment&) {
    deactivated_ = true;
    return true;
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

void bridge::whole(state s) {
    if (resource_status_memory_) {
        resource_status_memory_->whole(s);
    }
}

void bridge::wait_for_shutdown() {
    resource_status_memory_->wait_for_shutdown();
}

void bridge::mutex_file(std::string_view file_name) {
    if (resource_status_memory_) {
        resource_status_memory_->mutex_file(file_name);
    }
}

std::string_view bridge::mutex_file() {
    if (resource_status_memory_) {
        return resource_status_memory_->mutex_file();
    }
    return {};
}

void bridge::set_maximum_sessions(std::size_t n) {
    if (resource_status_memory_) {
        return resource_status_memory_->set_maximum_sessions(n);
    }
}

void bridge::add_shm_entry(std::size_t session_id, std::size_t index) {
    if (resource_status_memory_) {
        return resource_status_memory_->add_shm_entry(session_id, index);
    }
}

void bridge::set_digest(const std::string& path_string) {
    auto hash = std::hash<std::string>{}(path_string);
    std::ostringstream sstream;
    sstream << std::hex << std::setfill('0')
            << std::setw(sizeof(hash) * 2) << hash;
    digest_ = sstream.str();
}

std::string_view bridge::label() const noexcept {
    return component_label;
}

} // namespace tateyama::status_info::resource
