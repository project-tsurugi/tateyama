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

class cache_align environment {
public:

    /**
     * @brief create empty object
     */
     environment() = default;

    /**
     * @brief initialize environment with configuration file
     * @param mode framework boot mode on this environment
     * @param cfg configuration for the environment.
     */
    environment(boot_mode mode, std::shared_ptr<api::configuration::whole> cfg) noexcept;

    /**
     * @brief destruct the object
     */
    ~environment() = default;

    environment(environment const& other) = default;
    environment& operator=(environment const& other) = default;
    environment(environment&& other) noexcept = default;
    environment& operator=(environment&& other) noexcept = default;

    /**
     * @brief accessor to the framework boot mode
     * @return the framework boot mode
     */
    [[nodiscard]] boot_mode mode() const noexcept;

    /**
     * @brief accessor to the configuration
     * @return reference to the configuration created by initialize()
     * @pre initialize() has been called successfully
     */
    [[nodiscard]] std::shared_ptr<api::configuration::whole> const& configuration() const noexcept;

    /**
     * @brief accessor to the resource component repository
     */
    repository<resource>& resource_repository() noexcept;

    /**
     * @brief accessor to the service component repository
     */
    repository<service>& service_repository() noexcept;

    /**
     * @brief accessor to the endpoint component repository
     */
    repository<endpoint>& endpoint_repository() noexcept;

private:
    boot_mode mode_{};
    std::shared_ptr<api::configuration::whole> configuration_{};
    repository<resource> resource_repository_{};
    repository<service> service_repository_{};
    repository<endpoint> endpoint_repository_{};
};

}

