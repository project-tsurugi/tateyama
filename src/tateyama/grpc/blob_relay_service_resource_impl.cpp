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

#include "blob_session_impl.h"
#include "blob_relay_service_resource_impl.h"

namespace tateyama::grpc {

using namespace framework;

std::shared_ptr<blob_session> resource_impl::create_session(std::optional<blob_relay_service_resource::transaction_id_type> transaction_id) {
    return std::make_shared<blob_session>(std::unique_ptr<blob_session_impl, void(*)(blob_session_impl*)>(new blob_session_impl(blob_relay_service_->create_session(transaction_id)), [](blob_session_impl* e){ delete e; }));
}

component::id_type resource_impl::id() const noexcept {
    return tag;
}

bool resource_impl::setup(environment& env) {
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

bool resource_impl::start(environment& env) {
    auto datastore = env.resource_repository().find<tateyama::datastore::resource::bridge>();
    if (grpc_enabled_) {
        grpc_server_ = std::unique_ptr<server::grpc_server, void(*)(server::grpc_server*)>(new server::grpc_server(grpc_endpoint_), [](server::grpc_server* e){ delete e; } );

        // Create and add blob relay service to the server
        blob_relay_service_ = std::make_shared<blob_relay::blob_relay_service>(datastore->datastore(), env);
        grpc_server_->add_grpc_service_handler(blob_relay_service_);

        // Start the gRPC server
        grpc_server_thread_ = std::thread(std::ref(*grpc_server_));
    }
    return true;
}

bool resource_impl::shutdown(environment&) {
    if (grpc_enabled_) {
        grpc_server_->request_shutdown();

        if(grpc_server_thread_.joinable()) {
            grpc_server_thread_.join();
        }
    }
    return true;
}

resource_impl::~resource_impl() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

std::string_view resource_impl::label() const noexcept {
    return component_label;
}

}
