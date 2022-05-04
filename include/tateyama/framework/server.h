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
    explicit server(framework::boot_mode mode, std::shared_ptr<api::configuration::whole> cfg) :
        environment_(std::make_shared<environment>(mode, std::move(cfg)))
    {}

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
     * @brief start the server
     * @details setup and start all resource, service and endpoint appropriately.
     */
    void start();

    /**
     * @brief shutdown the server
     * @details shutdown all resource, service and endpoint appropriately.
     */
    void shutdown();

private:
    std::shared_ptr<environment> environment_{std::make_shared<environment>()};
};

/**
 * @brief install tateyama core components
 * @param svr the server to add comopnents to
 * @details This function add core components such as router, endpoint,
 * that are built-in part of tateyama infrastructure.
 */
void install_core_components(server& svr);

}

