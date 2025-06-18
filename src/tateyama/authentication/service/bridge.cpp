/*
 * Copyright 2018-2025 Project Tsurugi.
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

namespace tateyama::authentication::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment& env) {
    core_ = std::make_unique<core>(env.configuration());
    return true;
}

bool bridge::start(environment& env) {
    auto resource = env.resource_repository().find<tateyama::authentication::resource::bridge>();
    if (! resource) {
        LOG(ERROR) << "auth resource not found";
        return false;
    }
    return core_->start(resource.get());
}

bool bridge::shutdown(environment&) {
    return true;
}

bool bridge::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    return core_->operator()(req, res);
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

std::string_view bridge::label() const noexcept {
    return component_label;
}

}
