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
#pragma once

#include <functional>
#include <memory>
#include <type_traits>

#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/mode.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/api/configuration.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;

class server {
public:
    explicit server(api::configuration::whole&& conf, framework::mode);

    void add_resource(std::shared_ptr<resource>);
    void add_service(std::shared_ptr<service>);
    void add_endpoint(std::shared_ptr<endpoint>);

    resource& get_resource_by_id(component::id_type); // throws not_found
    template<class T> resource& get_resource(); // throws not_found

    void start(); // call .setup(), .start() for each components
    void shutdown(); // call .shutdown() for each components
};

}

