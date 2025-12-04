/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/system/service/core.h>

namespace tateyama::system::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief system service bridge for tateyama framework
 * @details This object bridges system as a service component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class system_service_bridge : public framework::service {
public:
    static constexpr id_type tag = framework::service_id_system;

    [[nodiscard]] id_type id() const noexcept override;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "system_service";

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(framework::environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(framework::environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(framework::environment&) override;

    bool operator()(
        std::shared_ptr<request> req,
        std::shared_ptr<response> res) override;

    /**
     * @brief create empty object
     */
    system_service_bridge() = default;

    system_service_bridge(system_service_bridge const& other) = delete;
    system_service_bridge& operator=(system_service_bridge const& other) = delete;
    system_service_bridge(system_service_bridge&& other) noexcept = delete;
    system_service_bridge& operator=(system_service_bridge&& other) noexcept = delete;

    /**
     * @brief destructor the object
     */
    ~system_service_bridge() override;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;
private:
    std::unique_ptr<tateyama::system::service::system_service_core> core_{};
    bool deactivated_{false};
};

}
