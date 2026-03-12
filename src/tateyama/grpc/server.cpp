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

#include <thread>

#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/logging_helper.h>

#include "server.h"

namespace tateyama::grpc {

/**
 * @brief gRPC server service
 */
tateyama_grpc_server::tateyama_grpc_server(std::string listen_address, std::vector<::grpc::Service*>& services, boost::barrier& sync,
                                           bool secure, const std::filesystem::path& fullchain_crt, const std::filesystem::path& server_key)
    : listen_address_(std::move(listen_address)), services_(services), sync_(sync), secure_(secure) {
    read_file(fullchain_crt, fullchain_crt_content_);
    read_file(server_key, server_key_content_);
}

void tateyama_grpc_server::operator()() {
    pthread_setname_np(pthread_self(), "grpc_server_impl");

    // Check services has been registered
    if (services_.empty()) {
        LOG_LP(INFO) << "The gRPC server did not start as no service has been registered";
        status_.store(status::no_service);
        sync_.wait();
        return;
    }

    std::shared_ptr<::grpc::ServerCredentials> creds{};
    if (secure_) {
        ::grpc::SslServerCredentialsOptions sslOpts{};
        sslOpts.pem_key_cert_pairs.push_back(::grpc::SslServerCredentialsOptions::PemKeyCertPair{ server_key_content_, fullchain_crt_content_ });
        creds = ::grpc::SslServerCredentials(sslOpts);
    } else {
        creds = ::grpc::InsecureServerCredentials();
    }
    // Build and start gRPC server with service added
    ::grpc::ServerBuilder builder{};
    // Set GRPC_ARG_ALLOW_REUSEPORT to 0 (off)
    builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
    // Set ListeningPort
    builder.AddListeningPort(listen_address_, creds);

    // Register gRpc service
    for (auto&& e : services_) {
        builder.RegisterService(e);
    }

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        LOG_LP(ERROR) << "Failed to start gRPC server on " << listen_address_;
        sync_.wait();
        return;
    }
    LOG_LP(INFO) << "The gRPC server started on " << listen_address_;
    status_.store(status::working);
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

void tateyama_grpc_server::request_shutdown() noexcept {
    shutdown_requested_.store(true);
}

tateyama_grpc_server::status tateyama_grpc_server::get_status() const noexcept {
    return status_.load();
}

void tateyama_grpc_server::read_file(const std::filesystem::path& filename, std::string& file_content) {
    using namespace std::literals::string_literals;

    std::string str_line;
    std::ifstream file(filename, std::ios::in);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open "s + filename.string());
    }
    while(getline(file, str_line)) {
        file_content += str_line;
        file_content += '\n';
    }
    file.close();
}

} // namespace
