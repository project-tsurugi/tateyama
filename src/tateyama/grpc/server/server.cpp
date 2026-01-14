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

#include <thread>

#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/logging_helper.h>

#include <limestone/api/datastore.h>

#include "server.h"

namespace tateyama::grpc::server {

/**
 * @brief gRPC server service
 */
tateyama_grpc_server::tateyama_grpc_server(std::string listen_address, boost::barrier& sync) : listen_address_(std::move(listen_address)), sync_(sync) {
}

void tateyama_grpc_server::operator()() {
    pthread_setname_np(pthread_self(), "grpc_server_impl");

    // Build and start gRPC server with service added
    ::grpc::ServerBuilder builder{};
    // Set GRPC_ARG_ALLOW_REUSEPORT to 0 (off)
    builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
    // Set ListeningPort
    builder.AddListeningPort(listen_address_, ::grpc::InsecureServerCredentials());

    // Register gRpc service
    for (auto&& e : handlers_) {
        e->register_to_builder(builder);
    }

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_LP(ERROR) << "Failed to start gRPC server on " << listen_address_;
        sync_.wait();
        return;
    }
    LOG_LP(INFO) << "The gRPC server started on " << listen_address_;
    working_.store(true);
    sync_.wait();

    // Wait for shutdown signal
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    LOG_LP(INFO) << "Shutdown request received. Stopping the gRPC server...";
    server->Shutdown();
    server->Wait();
    LOG_LP(INFO) << "The gRPC server stopped.";
}

void tateyama_grpc_server::add_grpc_service_handler(std::shared_ptr<grpc_service_handler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.emplace_back(std::move(handler));
}

void tateyama_grpc_server::request_shutdown() noexcept {
    shutdown_requested_.store(true);
}

bool tateyama_grpc_server::is_working() const noexcept {
    return working_.load();
}

} // namespace
