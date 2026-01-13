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
      * @brief add a service to the gRPC server..
      * @param service the service to be registered
      * @throw std::runtime_error if gRPC server is already started
      */
    void add_service(::grpc::Service* service);

    bool setup(framework::environment& env) override;

    bool start(framework::environment& env) override;

    bool shutdown(framework::environment& env) override;

    [[nodiscard]] framework::component::id_type id() const noexcept override;

    [[nodiscard]] std::string_view label() const noexcept override;

private:
    std::unique_ptr<resource_impl, void(*)(resource_impl*)> impl_;
};

} // namespace
