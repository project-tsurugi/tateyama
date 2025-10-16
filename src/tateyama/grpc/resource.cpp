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

#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/utils/boolalpha.h>
#include "logging.h"

#include <tateyama/datastore/resource/bridge.h>
#include "resource.h"
#include "blob_relay/blob_relay_service.h"

namespace tateyama::grpc {

using namespace framework;

component::id_type server_resource::id() const noexcept {
    return tag;
}

bool server_resource::setup(environment& env) {
    auto cfg = env.configuration();

    // grpc section
    auto* grpc_config = cfg->get_section("grpc");
    if (auto grpc_enabled_opt = grpc_config->get<bool>("enabled"); grpc_enabled_opt) {
        grpc_enabled_ = grpc_enabled_opt.value();
    }
    if (auto grpc_endpoint_opt = grpc_config->get<std::string>("endpoint"); grpc_endpoint_opt) {
        grpc_endpoint_ = grpc_endpoint_opt.value();
    }

    // output configuration to be used
    LOG(INFO) << tateyama::grpc::grpc_config_prefix
              << "grpc_enabled: " << utils::boolalpha(grpc_enabled_) << ", "
              << "gRPC service is enabled or not.";
    LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
              << "grpc_endpoint: " << grpc_endpoint_ << ", "
              << "endpoint address of the blob_relay service.";

    return true;
}

bool server_resource::start(environment& env) {
    auto datastore = env.resource_repository().find<tateyama::datastore::resource::bridge>();
    if (grpc_enabled_) {
        grpc_server_ = std::make_unique<server::grpc_server>(grpc_endpoint_);

        // Create and add blob relay service to the server
        grpc_server_->add_grpc_service_handler(
            std::make_shared<blob_relay::blob_relay_helper>(datastore->datastore(), env)
        );

        // Start the gRPC server
        grpc_server_thread_ = std::thread(std::ref(*grpc_server_));
    }
    return true;
}

bool server_resource::shutdown(environment&) {
    if (grpc_enabled_) {
        grpc_server_->request_shutdown();

        if(grpc_server_thread_.joinable()) {
            grpc_server_thread_.join();
        }
    }
    return true;
}

server_resource::~server_resource() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

std::string_view server_resource::label() const noexcept {
    return component_label;
}

}
