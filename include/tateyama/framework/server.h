/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <chrono>

#include <tateyama/framework/environment.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/boot_mode.h>
#include <tateyama/api/configuration.h>

namespace tateyama::framework {

/**
 * @brief interface for tateyama components life cycle management
 */
class server {
public:
    /**
     * @brief create new object
     */
    server() = default;

    /**
     * @brief destruct object
     */
    ~server() = default;

    server(server const& other) = delete;
    server& operator=(server const& other) = delete;
    server(server&& other) noexcept = delete;
    server& operator=(server&& other) noexcept = delete;

    /**
     * @brief create new object with environment
     * @param mode framework boot mode on this environment
     * @param cfg configuration for the environment.
     */
    server(framework::boot_mode mode, std::shared_ptr<api::configuration::whole> cfg);

    /**
     * @brief add new resource to manage life cycle
     * @details the priority between resource objects are determined by the order of addition.
     * The objects added earlier have higher priority.
     * @param arg tateyama resource
     */
    void add_resource(std::shared_ptr<resource> arg);

    /**
     * @brief add new service to manage life cycle
     * @details the priority between service objects are determined by the order of addition.
     * The objects added earlier have higher priority.
     * @param arg tateyama service
     */
    void add_service(std::shared_ptr<service> arg);

    /**
     * @brief add new endpoint to manage life cycle
     * @details the priority between endpoint objects are determined by the order of addition.
     * The objects added earlier have higher priority.
     * @param arg tateyama endpoint
     */
    void add_endpoint(std::shared_ptr<endpoint> arg);

    /**
     * @brief find the resource by resource id
     * @return the found resource
     * @return nullptr if not found
     */
    std::shared_ptr<resource> find_resource_by_id(component::id_type id);

    /**
     * @brief find the resource for the given type
     * @return the found resource
     * @return nullptr if not found
     */
    template<class T> std::shared_ptr<T> find_resource() {
        return environment_->resource_repository().find<T>();
    }

    /**
     * @brief find the service by service id
     * @return the found service
     * @return nullptr if not found
     */
    std::shared_ptr<service> find_service_by_id(component::id_type id);

    /**
     * @brief find the service for the given type
     * @return the found service
     * @return nullptr if not found
     */
    template<class T> std::shared_ptr<T> find_service() {
        return environment_->service_repository().find<T>();
    }

    /**
     * @brief setup the server
     * @details setup all resource, service and endpoint in this order appropriately.
     * Components on the same category are setup in the priority order. Highest priority one is setup first.
     */
    bool setup();

    /**
     * @brief start the server
     * @details setup (if not yet done) and start all resource, service and endpoint in this order appropriately.
     * Components on the same category are started in the priority order. Highest priority one is started first.
     */
    bool start();

    /**
     * @brief shutdown the server
     * @details shutdown all endpoint, service and resource in this order appropriately.
     * Components on the same category are shutdown in the reverse priority order. Highest priority one is shutdown last.
     */
    bool shutdown();

private:
    std::shared_ptr<environment> environment_{std::make_shared<environment>()};
    bool setup_done_{false};
#ifdef ENABLE_ALTIMETER
    std::chrono::time_point<std::chrono::steady_clock> db_start_time_{};
#endif
};

/**
 * @brief add tateyama core components
 * @param svr the server to add comopnents to
 * @details This function add core components such as router, endpoint,
 * that are built-in part of tateyama infrastructure.
 */
void add_core_components(server& svr);

}

