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

#include "server/ping_service/ping_service.h"
#include "blob_relay_service_resource_impl.h"

namespace tateyama::grpc {

using namespace framework;

bool resource_impl::setup(environment& env) {
    const auto& cfg = env.configuration();

    // grpc section
    if (auto* grpc_config = cfg->get_section("grpc_server"); grpc_config) {
        if (auto grpc_enabled_opt = grpc_config->get<bool>("enabled"); grpc_enabled_opt) {
            grpc_enabled_ = grpc_enabled_opt.value();
        }
        if (auto grpc_endpoint_opt = grpc_config->get<std::string>("endpoint"); grpc_endpoint_opt) {
            grpc_endpoint_ = grpc_endpoint_opt.value();
            port_ = grpc_endpoint_.substr(grpc_endpoint_.find_last_of(':') + 1);
        }
        if (auto grpc_secure_opt = grpc_config->get<bool>("secure"); grpc_secure_opt) {
            grpc_secure_ = grpc_secure_opt.value();
        }
    }

    if (grpc_enabled_) {
        // check blob_relay.enabled
        if (auto* blob_relay_config = cfg->get_section("blob_relay"); blob_relay_config) {
            if (auto blob_relay_enabled_opt = blob_relay_config->get<bool>("enabled"); blob_relay_enabled_opt) {
                blob_relay_enabled_ = blob_relay_enabled_opt.value();
                if (blob_relay_enabled_) {
                    // create the relay service
                    service_handler_ = std::make_shared<blob_relay::blob_relay_service_handler>();
                }
            }
        }
    }

    // output configuration to be used
    LOG(INFO) << tateyama::grpc::grpc_config_prefix
              << "grpc_enabled: " << utils::boolalpha(grpc_enabled_) << ", "
              << "gRPC service is enabled or not.";
    LOG(INFO) << tateyama::grpc::grpc_config_prefix
              << "grpc_endpoint: " << grpc_endpoint_ << ", "
              << "endpoint address of the grpc server.";
    LOG(INFO) << tateyama::grpc::grpc_config_prefix
              << "grpc_endpoint: " << grpc_secure_ << ", "
              << "endpoint address of the grpc server.";

    // output configuration to be used
    LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
              << "blob_relay_enabled: " << utils::boolalpha(blob_relay_enabled_) << ", "
              << "blob relay service is enabled or not.";

    return true;
}

bool resource_impl::start(environment& env) {
    if (grpc_enabled_) {
        try {
            grpc_server_ = std::unique_ptr<server::tateyama_grpc_server, void(*)(server::tateyama_grpc_server*)>(new server::tateyama_grpc_server(port_), [](server::tateyama_grpc_server* e){ delete e; } );  // NOLINT

            if (service_handler_) {
                // start blob relay service and add it to the server
                if (!service_handler_->start(env)) {
                    LOG(ERROR) << "cannot start the lob relay service";
                    return false;
                }
                grpc_server_->add_grpc_service_handler(service_handler_);
            }

            // Start the gRPC server
            grpc_server_thread_ = std::thread(std::ref(*grpc_server_));

            // Wait until the server is ready (using ping_service)
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
        if (grpc_server_) {
            grpc_server_->request_shutdown();
        }

        if(grpc_server_thread_.joinable()) {
            grpc_server_thread_.join();
        }
    }
    return true;
}

resource_impl::~resource_impl() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << blob_relay_service_resource::component_label;
};

std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> resource_impl::blob_relay_service() {
    if(service_handler_) {
        return service_handler_->blob_relay_service();
    }
    throw std::runtime_error("blob_relay_service is not ready. Do this after start()");
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
    auto pos = grpc_endpoint_.find(':');
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
