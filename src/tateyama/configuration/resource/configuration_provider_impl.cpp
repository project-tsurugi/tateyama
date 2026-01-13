/*
 * Copyright 2026-2026 Project Tsurugi.
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

#include "configuration_provider_impl.h"

namespace tateyama::configuration::resource {

api::server::database_info const& configuration_provider_impl::database_info() const noexcept {
    return *database_info_;
}

bool configuration_provider_impl::setup(framework::environment& env) {
    const auto& conf = env.configuration();

    // database name
    auto* ipc_section = conf->get_section("ipc_endpoint");
    auto database_name_opt = ipc_section->get<std::string>("database_name");
    if (!database_name_opt) {
        LOG(ERROR) << "cannot find database_name at the ipc_endpoint section in the configuration";
        return false;
    }

    // instance_id
    auto* system_section = conf->get_section("system");
    auto instance_id_opt = system_section->get<std::string>("instance_id");
    if (!instance_id_opt) {
        LOG(ERROR) << "cannot find  instance_id at the system section in the configuration";
        return false;
    }

    // create database_info
    database_info_ = std::make_unique<database_info_impl>(database_name_opt.value(), instance_id_opt.value());

    return database_info_ != nullptr;
}

bool configuration_provider_impl::start(framework::environment&) {
    return true;
}

bool configuration_provider_impl::shutdown(framework::environment&) {
    return true;
}

tateyama::framework::component::id_type configuration_provider_impl::id() const noexcept {
    return tag;
}

std::string_view configuration_provider_impl::label() const noexcept {
    return component_label;
}

}
