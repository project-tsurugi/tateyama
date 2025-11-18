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

#include "ping_service/ping_service.h"
#include "server.h"

namespace tateyama::grpc::server {

/**
 * @brief gRPC server service
 */
tateyama_trpc_server::tateyama_trpc_server(std::string grpc_endpoint) : grpc_endpoint_(std::move(grpc_endpoint)) {
}

void tateyama_trpc_server::operator()() {
    pthread_setname_np(pthread_self(), "grpc_server_impl");

    // Build and start gRPC server with service added
    ::grpc::ServerBuilder builder{};
    builder.AddListeningPort(grpc_endpoint_, ::grpc::InsecureServerCredentials());

    // Register ping service
    tateyama::grpc::server::ping_service::ping_service ping_service;
    builder.RegisterService(&ping_service);

    // Register gRpc service
    for (auto&& e : handlers_) {
        e->register_to_builder(builder);
    }

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_LP(ERROR) << "Failed to start gRPC server on " << grpc_endpoint_;
        return;
    }
    LOG_LP(INFO) << "The gRPC server started on " << grpc_endpoint_;

    // Wait for shutdown signal
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    LOG_LP(INFO) << "Shutdown request received. Stopping the gRPC server...";
    server->Shutdown();
    server->Wait();
    LOG_LP(INFO) << "The gRPC server stopped.";
}

void tateyama_trpc_server::add_grpc_service_handler(std::shared_ptr<grpc_service_handler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.emplace_back(std::move(handler));
}

void tateyama_trpc_server::request_shutdown() {
    shutdown_requested_.store(true);
}

} // namespace
