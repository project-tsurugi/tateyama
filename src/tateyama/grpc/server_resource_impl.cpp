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

#include <fstream>

#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/utils/boolalpha.h>
#include "logging.h"

#include "server_resource_impl.h"

namespace tateyama::grpc {

using namespace framework;

bool resource_impl::setup(environment& env) {
    const auto& cfg = env.configuration();

    // grpc section
    if (auto* grpc_config = cfg->get_section("grpc_server"); grpc_config) {
        if (auto grpc_enabled_opt = grpc_config->get<bool>("enabled"); grpc_enabled_opt) {
            grpc_enabled_ = grpc_enabled_opt.value();
        }
        if (auto grpc_listen_address_opt = grpc_config->get<std::string>("listen_address"); grpc_listen_address_opt) {
            grpc_listen_address_ = grpc_listen_address_opt.value();
        }
        if (auto grpc_secure_opt = grpc_config->get<bool>("secure"); grpc_secure_opt) {
            grpc_secure_ = grpc_secure_opt.value();
        }

        if (grpc_secure_) {
            if (auto grpc_fullchain_crt_opt = grpc_config->get<std::filesystem::path>("fullchain_crt"); grpc_fullchain_crt_opt) {
                fullchain_crt_ = grpc_fullchain_crt_opt.value();
            } else {
                LOG(ERROR) << "fullchain_crt is not specified";
                return false;
            }
            if (auto grpc_server_key_opt = grpc_config->get<std::filesystem::path>("server_key"); grpc_server_key_opt) {
                server_key_ = grpc_server_key_opt.value();
            } else {
                LOG(ERROR) << "server_key is not specified";
                return false;
            }
        }

        // output configuration to be used
        LOG(INFO) << tateyama::grpc::grpc_config_prefix
                  << "enabled: " << utils::boolalpha(grpc_enabled_) << ", "
                  << "gRPC server is enabled or not.";
        LOG(INFO) << tateyama::grpc::grpc_config_prefix
                  << "listen_address: " << grpc_listen_address_ << ", "
                  << "listen address of the gRPC server.";
        LOG(INFO) << tateyama::grpc::grpc_config_prefix
                  << "secure: " << utils::boolalpha(grpc_secure_) << ", "
                  << "enable secure ports for gRPC server or not.";
        if (grpc_secure_) {
            LOG(INFO) << tateyama::grpc::grpc_config_prefix
                      << "fullchain_crt: " << fullchain_crt_.string() << ", "
                      << "file path of fullchain crt.";
            LOG(INFO) << tateyama::grpc::grpc_config_prefix
                      << "server_key: " << server_key_.string() << ", "
                      << "file path of server key.";
        }
    }
    return true;
}

bool resource_impl::start(environment&) {
    started_ = true;
    if (grpc_enabled_) {
        try {
            grpc_server_ = std::make_unique<tateyama_grpc_server>(grpc_listen_address_, services_, sync_, grpc_secure_, fullchain_crt_, server_key_);

            // Start the gRPC server
            grpc_server_thread_ = std::thread(std::ref(*grpc_server_));

            // Wait until the server is ready (using ping_service)
            sync_.wait();
            auto server_status = grpc_server_->get_status();
            if (server_status != tateyama_grpc_server::status::no_service &&
                server_status != tateyama_grpc_server::status::working) {
                LOG(ERROR) << "fail to launch gRPC server";
                return false;
            }
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
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << grpc_server_resource::component_label;
};

void resource_impl::add_service(::grpc::Service* service) {
    if (started_) {
        throw std::runtime_error("The gRPC server has already started");
    }
    services_.emplace_back(service);
}

}
