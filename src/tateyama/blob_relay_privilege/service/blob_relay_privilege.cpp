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

#include "blob_relay_privilege.h"

namespace tateyama::blob_relay_privilege::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

using namespace framework;

component::id_type blob_relay_privilege::id() const noexcept {
    return tag;
}

bool blob_relay_privilege::setup(environment& env) {
    core_ = std::make_unique<core>(env);
    return true;
}

bool blob_relay_privilege::start(environment& env) {
    return core_->start(env);
}

bool blob_relay_privilege::shutdown(environment&) {
    return true;
}

bool blob_relay_privilege::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    return core_->operator()(req, res);
}

std::string_view blob_relay_privilege::label() const noexcept {
    return component_label;
}

blob_relay_privilege::~blob_relay_privilege() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

}
