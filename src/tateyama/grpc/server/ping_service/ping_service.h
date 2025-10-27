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

#include <grpcpp/grpcpp.h>

#include <tateyama/grpc/server/service_handler.h>
#include <tateyama/proto/grpc/ping_service.grpc.pb.h>

namespace tateyama::grpc::server::ping_service {

using tateyama::grpc::proto::PingService;
using tateyama::grpc::proto::PingRequest;
using tateyama::grpc::proto::PingResponse;

class ping_service final : public PingService::Service {
public:
    ::grpc::Status Ping(::grpc::ServerContext*, const PingRequest*, PingResponse*) override {
        return ::grpc::Status::OK;
    }
};

class ping_service_handler : public tateyama::grpc::server::grpc_service_handler {
public:
    ping_service_handler() : ping_service_(std::make_unique<ping_service>()) {
    }

    void register_to_builder(::grpc::ServerBuilder& builder) override {
        builder.RegisterService(ping_service_.get());
    }

private:
    std::unique_ptr<ping_service> ping_service_;
};

}
