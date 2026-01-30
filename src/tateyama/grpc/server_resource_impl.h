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

#include <boost/thread/barrier.hpp>

#include <tateyama/framework/component_ids.h>
#include <tateyama/grpc/server_resource.h>
#include "server.h"

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
     * @brief destructor of this object
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

    void add_service(::grpc::Service* service);

private:
    std::unique_ptr<tateyama_grpc_server> grpc_server_{};
    std::thread grpc_server_thread_{};
    std::vector<::grpc::Service*> services_{};

    std::string grpc_listen_address_{};
    boost::barrier sync_{2};
    bool grpc_enabled_{};
    bool grpc_secure_{};
    bool started_{};
};

} // namespace
