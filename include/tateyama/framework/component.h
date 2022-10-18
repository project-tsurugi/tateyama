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

#include <tateyama/utils/cache_align.h>
#include <tateyama/framework/component.h>

namespace tateyama::framework {

class environment;

/**
 * @brief base class for tateyama components whose life-cycle is managed by the framework
 */
class component {
public:
    /**
     * @brief type to identify components
     * @details the id must be unique among each categories of components (e.g. resource, service)
     * Users custom module should use one larger than `max_system_reserved_id`.
     */
    using id_type = std::uint32_t;

    /**
     * @brief maximum id reserved for built-in system resources/services
     */
    static constexpr id_type max_system_reserved_id = 255;

    /**
     * @brief construct new object
     */
    component() = default;

    component(component const& other) = delete;
    component& operator=(component const& other) = delete;
    component(component&& other) noexcept = delete;
    component& operator=(component&& other) noexcept = delete;

    /**
     * @brief setup the component (the state will be `ready`)
     * @return true when setup completed successfully
     * @return false otherwise
     */
    virtual bool setup(environment&) = 0;

    /**
     * @brief start the component (the state will be `activated`)
     * @return true when start completed successfully
     * @return false otherwise
     */
    virtual bool start(environment&) = 0;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     * @return true when shutdown completed successfully, or component is already deactivated
     * @return false otherwise
     * @note shutdown is an idempotent operation, meaning second call to the already deactivated component should be
     * simply ignored and return true.
     */
    virtual bool shutdown(environment&) = 0;

    /**
     * @brief destruct the object (the state will be `disposed`)
     */
    virtual ~component() = default;

    /**
     * @brief list the section names in the config. file that this component is affected
     * @return the list of section name
     */
    // TODO implement to validate config file
    //virtual std::vector<std::string> configuration_sections() = 0;
};

}

