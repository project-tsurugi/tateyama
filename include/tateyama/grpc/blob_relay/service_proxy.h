/*
 * Copyright 2025-2026 Project Tsurugi.
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

#include <grpcpp/grpcpp.h>
#include <vector>
#include <optional>
#include <thread>
#include <cstdint>
#include <memory>

#include <limestone/api/datastore.h>
#include <tateyama/datastore/resource/bridge.h>

#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

#include <data_relay_grpc/blob_relay/service.h>

namespace tateyama::grpc::blob_relay {

class service_proxy_impl;

class service_proxy : public framework::resource {
public:
   /**
     * @brief Returns a data_relay_grpc::blob_relay::blob_relay_service
     * @return the shared_ptr of data_relay_grpc::blob_relay::blob_relay_service
     */
    [[nodiscard]] std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service();

    static constexpr framework::component::id_type tag = framework::resource_id_blob_relay_service;

    [[nodiscard]] framework::component::id_type id() const noexcept override;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "blob_relay_service_proxy";

    [[nodiscard]] std::string_view label() const noexcept override;

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

    /**
     * @brief constructor of this object.
     */
    service_proxy();

    /**
     * @brief destructor of this object
     */
    ~service_proxy() override;

    service_proxy(service_proxy const& other) = delete;
    service_proxy& operator=(service_proxy const& other) = delete;
    service_proxy(service_proxy&& other) noexcept = delete;
    service_proxy& operator=(service_proxy&& other) noexcept = delete;

private:
    std::unique_ptr<service_proxy_impl, void(*)(service_proxy_impl*)> impl_;
};

} // namespace
