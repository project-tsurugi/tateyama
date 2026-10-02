/*
 * Copyright 2026-2026 Project Tsurugi.
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

#include "service_adapter_impl.h"

#include <glog/logging.h>

#include <tateyama/grpc/server_resource.h>

namespace tateyama::grpc::distributed_query {

service_adapter_impl::service_adapter_impl() = default;

service_adapter_impl::~service_adapter_impl() = default;

bool service_adapter_impl::setup(framework::environment& env) {
    bool grpc_enabled{false};
    const auto& cfg = env.configuration();
    if (auto* grpc_config = cfg->get_section("grpc_server"); grpc_config) {
        if (auto grpc_enabled_opt = grpc_config->get<bool>("enabled"); grpc_enabled_opt) {
            grpc_enabled = grpc_enabled_opt.value();
        }
    }
    if (!grpc_enabled) {
        return true;
    }

    auto server_resource =
        env.resource_repository().find<tateyama::grpc::grpc_server_resource>();
    if (!server_resource) {
        LOG(ERROR) << "cannot find the grpc_server_resource";
        return false;
    }

    service_ = std::make_shared<service>();
    service_->set_remote_process_handler(handler_);
    server_resource->add_service(service_.get());
    return true;
}

void service_adapter_impl::set_remote_process_handler(std::shared_ptr<remote_process_handler> handler) {
    handler_ = std::move(handler);
    if (service_) {
        service_->set_remote_process_handler(handler_);
    }
}

} // namespace tateyama::grpc::distributed_query
