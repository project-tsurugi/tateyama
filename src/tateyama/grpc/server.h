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
#pragma once

#include <memory>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <vector>
#include <mutex>

#include <grpcpp/grpcpp.h>
#include <boost/thread/barrier.hpp>

#include <tateyama/framework/environment.h>

namespace tateyama::grpc {

/**
 * @brief gRPC server
 */
class tateyama_grpc_server {
public:
    /**
     * @brief the server status type.
     */
    enum class status : std::int32_t {
        /**
         * @brief initial state.
         */
        init = 0,

        /**
         * @brief no service has been retistered, thus gRPC server has not launched.
         */
        no_service,

        /**
         * @brief the gRPC server has successfully launched.
         */
        working
    };

    explicit tateyama_grpc_server(std::string listen_address, std::vector<::grpc::Service*>& services, boost::barrier& sync);

    /**
     * @brief Processing core of the gRPC server
     */
    void operator()();

    /**
     * @brief request the gRPC server shutdown
     */
    void request_shutdown() noexcept;

    /**
     * @brief return current server status
     * @return the status of the server
     */
    [[nodiscard]] tateyama_grpc_server::status get_status() const noexcept;

private:
    std::string listen_address_;
    std::vector<::grpc::Service*>& services_;
    boost::barrier& sync_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<status> status_{status::init};

    ::grpc::ServerBuilder builder_{};
    mutable std::mutex mutex_{};
};

} // namespace
