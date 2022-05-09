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
#include <tateyama/framework/endpoint_broker.h>

#include <functional>
#include <memory>
#include <type_traits>

#include <takatori/util/fail.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/service.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;

using takatori::util::fail;

component::id_type endpoint_broker::id() const noexcept {
    return tag;
}

void endpoint_broker::operator()(std::shared_ptr<request>, std::shared_ptr<response>) {
    fail();
}
}

