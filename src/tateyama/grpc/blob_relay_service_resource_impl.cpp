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
#include "server/ping_service/ping_service.h"
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

        // Wait until the server is ready (using ping_service)
        try {
            wait_for_server_ready();
        } catch (std::runtime_error &ex) {
            LOG(ERROR) << ex.what();
            return false;
        }
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

// Wait until the server is ready (using ping_service)
void resource_impl::wait_for_server_ready() {
    constexpr int max_attempts = 50;
    constexpr int wait_millis = 10;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        if (is_server_ready()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_millis));
    }
    throw std::runtime_error("gRPC server did not become ready in time");
}

// Use ping_service to check if server is ready
bool resource_impl::is_server_ready() {
    auto pos = grpc_endpoint_.find(":");
    if (pos == std::string::npos) {
        throw std::runtime_error("server address does not includes port number");
    }
    std::string address("localhost");
    address += grpc_endpoint_.substr(pos);
    auto channel = ::grpc::CreateChannel(grpc_endpoint_, ::grpc::InsecureChannelCredentials());
    tateyama::grpc::proto::PingService::Stub stub(channel);
    ::grpc::ClientContext context;
    tateyama::grpc::proto::PingRequest req;
    tateyama::grpc::proto::PingResponse resp;
    auto status = stub.Ping(&context, req, &resp);
    return status.ok();
}

}
