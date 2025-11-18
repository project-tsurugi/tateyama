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

#include <cstdint>
#include <optional>
#include <memory>
#include <thread>

#include <tateyama/framework/component_ids.h>

#include <tateyama/grpc/blob_relay_service_resource.h>
#include "blob_relay/blob_relay_service.h"
#include "server/server.h"

namespace tateyama::grpc {

class resource_impl {
public:
    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(framework::environment&);

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(framework::environment&);

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(framework::environment&);

    /**
     * @brief destructor the object
     */
    ~resource_impl();

    resource_impl(resource_impl const& other) = delete;
    resource_impl& operator=(resource_impl const& other) = delete;
    resource_impl(resource_impl&& other) noexcept = delete;
    resource_impl& operator=(resource_impl&& other) noexcept = delete;

    /**
     * @brief constructor of this object.
     */
    resource_impl() = default;

    /**
      * @brief Provides the data_relay_grpc::blob_relay_service.
      * @return the blob_relay_service object
      */
    [[nodiscard]] std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service();

private:
    std::unique_ptr<server::tateyama_grpc_server, void(*)(server::tateyama_grpc_server*)> grpc_server_{nullptr, [](server::tateyama_grpc_server*){} };
    std::thread grpc_server_thread_{};
    std::shared_ptr<blob_relay::blob_relay_service_handler> service_handler_{};

    bool grpc_enabled_{};
    std::string grpc_endpoint_{};

    void wait_for_server_ready();
    bool is_server_ready();
};

} // namespace
