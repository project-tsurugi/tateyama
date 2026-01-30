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

#include <tateyama/grpc/blob_relay/service_proxy.h>
#include <data_relay_grpc/blob_relay/service.h>

namespace tateyama::grpc::blob_relay {

class service_proxy_impl {
public:
   /**
     * @brief Returns a data_relay_grpc::blob_relay::blob_relay_service
     * @return the shared_ptr of data_relay_grpc::blob_relay::blob_relay_service
     */
    [[nodiscard]] std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service();

    bool setup(framework::environment& env);

    /**
     * @brief constructor of this object.
     */
    service_proxy_impl();

    /**
     * @brief destructor of this object
     */
    ~service_proxy_impl();

    service_proxy_impl(service_proxy_impl const& other) = delete;
    service_proxy_impl& operator=(service_proxy_impl const& other) = delete;
    service_proxy_impl(service_proxy_impl&& other) noexcept = delete;
    service_proxy_impl& operator=(service_proxy_impl&& other) noexcept = delete;
    
private:
    std::shared_ptr<::tateyama::datastore::resource::bridge> datastore_resource_{};
    std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> service_handler_{};
    std::unique_ptr<data_relay_grpc::blob_relay::blob_relay_service::api> api_{};

    bool blob_relay_enabled_{};
};

} // namespace
