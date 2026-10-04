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
#include <memory>
#include <string>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::grpc {

class resource_impl;

class grpc_server_resource : public framework::resource {
public:
    static constexpr id_type tag = framework::resource_id_grpc_server;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "grpc_server_resource";

    /**
     * @brief create object
     */
    grpc_server_resource();
    
    /**
     * @brief register an owned group of services before listener startup.
     * @param key stable unique name of the registration source
     * @param services services with ownership of all objects they depend on
     * @details Repeating the same key, pointers, and owners is a no-op.
     * Aliasing shared pointers may retain an aggregate service owner.
     * Registrations survive failed setup; shutdown after startup releases them.
     * @throw std::invalid_argument for empty/null registrations or conflicts
     * @throw std::runtime_error after listener startup
     */
    void add_services(std::string key, std::vector<std::shared_ptr<::grpc::Service>> services);

    bool setup(framework::environment& env) override;

    bool start(framework::environment& env) override;

    bool shutdown(framework::environment& env) override;

    [[nodiscard]] framework::component::id_type id() const noexcept override;

    [[nodiscard]] std::string_view label() const noexcept override;

private:
    std::unique_ptr<resource_impl, void(*)(resource_impl*)> impl_;
};

} // namespace
