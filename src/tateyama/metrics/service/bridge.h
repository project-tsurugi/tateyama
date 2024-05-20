/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include <tateyama/metrics/resource/bridge.h>
#include <tateyama/metrics/service/core.h>

namespace tateyama::metrics::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief metrics resource bridge for tateyama framework
 * @details This object bridges metrics as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class bridge : public framework::service {
public:
    static constexpr id_type tag = framework::service_id_metrics;

    [[nodiscard]] id_type id() const noexcept override;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "metrics_service";

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
    bridge() = default;

    bridge(bridge const& other) = delete;
    bridge& operator=(bridge const& other) = delete;
    bridge(bridge&& other) noexcept = delete;
    bridge& operator=(bridge&& other) noexcept = delete;

    /**
     * @brief destructor the object
     */
    ~bridge() override;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

private:
    std::unique_ptr<tateyama::metrics::service::core> core_{};
    bool deactivated_{false};
};

}
