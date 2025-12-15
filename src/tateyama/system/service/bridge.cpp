/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include "bridge.h"

namespace tateyama::system::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

using namespace framework;

component::id_type system_service_bridge::id() const noexcept {
    return tag;
}

bool system_service_bridge::setup(environment&) {
    core_ = std::make_unique<system_service_core>();
    try {
        LOG_LP(INFO) << "tsurugidb system info is '" << core_->info_handler_.to_string() << '\'';
    } catch (const std::runtime_error &ex) {
        LOG_LP(INFO) << ex.what();
    }
    return core_ != nullptr;
}

bool system_service_bridge::start(environment&) {
    return true;
}

bool system_service_bridge::shutdown(environment&) {
    return true;
}

bool system_service_bridge::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    return core_->operator()(req, res);
}

std::string_view system_service_bridge::label() const noexcept {
    return component_label;
}

system_service_bridge::~system_service_bridge() = default;

} // namespace
