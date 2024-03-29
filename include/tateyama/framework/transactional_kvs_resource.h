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

#include <sharksfin/api.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::framework {

/**
 * @brief resource component
 */
class transactional_kvs_resource : public resource {
public:
    static constexpr id_type tag = resource_id_transactional_kvs;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "transactional_kvs_resource";

    /**
     * @brief create empty object
     */
    transactional_kvs_resource() = default;

    transactional_kvs_resource(transactional_kvs_resource const& other) = delete;
    transactional_kvs_resource& operator=(transactional_kvs_resource const& other) = delete;
    transactional_kvs_resource(transactional_kvs_resource&& other) noexcept = delete;
    transactional_kvs_resource& operator=(transactional_kvs_resource&& other) noexcept = delete;

    /**
     * @brief destruct object
     */
    ~transactional_kvs_resource() override;

    /**
     * @brief create new object
     */
    explicit transactional_kvs_resource(sharksfin::DatabaseHandle handle) noexcept;

    /**
     * @brief retrieve id
     * @return the component id of this resource. Unique among resources.
     */
    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(environment&) override;

    /**
     * @brief accessor to the native handle
     */
    [[nodiscard]] sharksfin::DatabaseHandle core_object() const noexcept;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;
private:
    sharksfin::DatabaseHandle database_handle_{};
    bool db_opened_{};
};

}

