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
#include <tateyama/framework/routing_service.h>

#include <functional>
#include <memory>
#include <type_traits>

#include <takatori/util/fail.h>

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/ids.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;
using takatori::util::fail;

component::id_type routing_service::id() const noexcept {
    return tag;
}

void routing_service::setup(environment& env) {
    services_ = std::addressof(env.service_repository());
}

void routing_service::start(environment&) {
    //no-op
}

void routing_service::shutdown(environment&) {
    services_ = {};
}

void routing_service::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    if (services_ == nullptr) {
        fail();
    }
    if (req->service_id() == tag) {
        fail();
    }
    if (auto destination = services_->find_by_id(req->service_id()); destination != nullptr) {
        destination->operator()(std::move(req), std::move(res));
        return;
    }
    // wrong address
    fail();
}

}

