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
#include <tateyama/datastore/service/bridge.h>

#include <functional>
#include <memory>
#include <type_traits>

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/datastore/resource/bridge.h>

namespace tateyama::datastore::service {

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
    auto resource = env.resource_repository().find<tateyama::datastore::resource::bridge>();
    if (! resource) {
        LOG(ERROR) << "datastore resource not found";
        return false;
    }
    return core_->start(resource->core_object());
}

bool bridge::shutdown(environment&) {
    return core_->shutdown();
}

bool bridge::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    return core_->operator()(std::move(req), std::move(res));
}

bridge::~bridge() {
    if(core_ && ! deactivated_) {
        core_->shutdown(true);
    }
}

}

