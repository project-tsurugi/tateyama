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

namespace tateyama::server {

using tateyama::framework::environment;
using tateyama::framework::component;
using tateyama::framework::resource;
using tateyama::framework::service;
using tateyama::framework::endpoint;

/**
 * @brief interface for tateyama components life cycle management
 */
class mock_server {
public:
    /**
     * @brief create new object
     */
    mock_server() = default;

    /**
     * @brief destruct object
     */
    ~mock_server() = default;

    mock_server(mock_server const& other) = delete;
    mock_server& operator=(mock_server const& other) = delete;
    mock_server(mock_server&& other) noexcept = delete;
    mock_server& operator=(mock_server&& other) noexcept = delete;

    /**
     * @brief create new object with environment
     * @param mode framework boot mode on this environment
     */
    mock_server(framework::boot_mode mode, std::shared_ptr<api::configuration::whole> cfg);

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
     * @brief setup the mock_server
     * @details setup all resource, service and endpoint in this order appropriately.
     * Components on the same category are setup in the priority order. Highest priority one is setup first.
     */
    bool setup();

    /**
     * @brief start the mock_server
     * @details setup (if not yet done) and start all resource, service and endpoint in this order appropriately.
     * Components on the same category are started in the priority order. Highest priority one is started first.
     */
    bool start();

    /**
     * @brief shutdown the mock_server
     * @details shutdown all endpoint, service and resource in this order appropriately.
     * Components on the same category are shutdown in the reverse priority order. Highest priority one is shutdown last.
     */
    bool shutdown();

private:
    std::shared_ptr<environment> environment_{std::make_shared<environment>()};
    std::shared_ptr<api::configuration::whole> cfg_{};
    bool setup_done_{false};
};

/**
 * @brief add tateyama core components
 * @param svr the mock_server to add comopnents to
 * @details This function add core components such as router, endpoint,
 * that are built-in part of tateyama infrastructure.
 */
void add_core_components(mock_server& svr);

}
