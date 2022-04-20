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

#include <tateyama/framework/repository.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/api/configuration.h>
#include <tateyama/framework/boot_mode.h>

namespace tateyama::framework {

class environment {
public:
    /**
     * @brief create new object
     */
    environment() = default;

    /**
     * @brief destruct the object
     */
    ~environment() = default;

    environment(environment const& other) = default;
    environment& operator=(environment const& other) = default;
    environment(environment&& other) noexcept = default;
    environment& operator=(environment&& other) noexcept = default;

    /**
     * @brief initialize environment with configuration file
     * @param cfg configuration for the environment. Specify nullptr to use default.
     */
    void initialize(boot_mode mode, std::shared_ptr<api::configuration::whole> cfg = {});

    /**
     * @brief accessor to the framework boot mode
     * @return the framework boot mode
     */
    boot_mode mode();

    /**
     * @brief accessor to the configuration
     * @return reference to the configuration created by initialize()
     * @pre initialize() has been called successfully
     */
    std::shared_ptr<api::configuration::whole> const& configuration();

    /**
     * @brief accessor to the resource component repository
     */
    repository<resource>& resource_repository();

    /**
     * @brief accessor to the service component repository
     */
    repository<service>& service_repository();

    /**
     * @brief accessor to the endpoint component repository
     */
    repository<endpoint>& endpoint_repository();

private:
    boot_mode mode_{};
    std::shared_ptr<api::configuration::whole> configuration_{};
    repository<resource> resource_repository_{};
    repository<service> service_repository_{};
    repository<endpoint> endpoint_repository_{};
};

}

