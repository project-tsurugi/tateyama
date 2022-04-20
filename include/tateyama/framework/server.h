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
#include <tateyama/framework/boot_mode.h>
#include <tateyama/api/configuration.h>

namespace tateyama::framework {

class server {
public:
    server() = default;

    server(server const& other) = delete;
    server& operator=(server const& other) = delete;
    server(server&& other) noexcept = delete;
    server& operator=(server&& other) noexcept = delete;

    explicit server(api::configuration::whole const& conf, framework::boot_mode) {
        //TODO
    }

    void add_resource(std::shared_ptr<resource>) {
        //TODO
    }
    void add_service(std::shared_ptr<service>) {
        //TODO
    }
    void add_endpoint(std::shared_ptr<endpoint>) {
        //TODO
    }

    std::shared_ptr<resource> get_resource_by_id(component::id_type) {
        return nullptr;
    }

    template<class T> std::shared_ptr<resource> get_resource() {
        return nullptr;
    }

    void start() {
        //TODO
        // call .setup(), .start() for each components
    }
    void shutdown() {
        //TODO
        // call .shutdown() for each components
    }
};

}

