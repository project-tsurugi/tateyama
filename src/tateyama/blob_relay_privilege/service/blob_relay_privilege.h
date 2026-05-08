/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/blob_relay_privilege/service/core.h>

namespace tateyama::blob_relay_privilege::service {

/**
 * @brief blob_relay_privilege resource blob_relay_privilege for tateyama framework
 * @details This object blob_relay_privileges blob_relay_privilege as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class blob_relay_privilege : public framework::service {
public:
    static constexpr id_type tag = framework::service_id_blob_relay_privilege;

    [[nodiscard]] id_type id() const noexcept override;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "blob_relay_privilege_service";

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
    blob_relay_privilege() = default;

    blob_relay_privilege(blob_relay_privilege const& other) = delete;
    blob_relay_privilege& operator=(blob_relay_privilege const& other) = delete;
    blob_relay_privilege(blob_relay_privilege&& other) noexcept = delete;
    blob_relay_privilege& operator=(blob_relay_privilege&& other) noexcept = delete;

    /**
     * @brief destructor the object
     */
    ~blob_relay_privilege() override;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;
private:
    std::unique_ptr<tateyama::blob_relay_privilege::service::core> core_{};
};

}
