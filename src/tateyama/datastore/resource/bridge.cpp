/*
 * Copyright 2018-2022 tsurugi project.
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
#include "tateyama/datastore/resource/bridge.h"

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::datastore::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

void bridge::setup(environment& env) {
    core_ = std::make_unique<core>(env.configuration());
}

void bridge::start(environment&) {
    core_->start();
}

void bridge::shutdown(environment&) {
    core_->shutdown();
    deactivated_ = true;
}

bridge::~bridge() {
    if(core_ && ! deactivated_) {
        core_->shutdown(true);
    }
}

core* bridge::core_object() const noexcept {
    return core_.get();
}

}

