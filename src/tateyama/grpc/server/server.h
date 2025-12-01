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

#include <memory>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <vector>
#include <mutex>

#include <tateyama/framework/environment.h>

#include"service_handler.h"

namespace tateyama::grpc::server {

/**
 * @brief gRPC server
 */
class tateyama_grpc_server {
public:
    explicit tateyama_grpc_server(std::string port);

    void operator()();

    void request_shutdown();

    /**
     * @brief add a gRPC service handler
     */
    void add_grpc_service_handler(std::shared_ptr<grpc_service_handler>);

private:
    std::string port_;
    std::atomic<bool> shutdown_requested_{false};

    std::mutex mutex_{};
    std::vector<std::shared_ptr<grpc_service_handler>> handlers_{};
};

} // namespace
