/*
 * Copyright 2026-2026 Project Tsurugi.
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

#include <tateyama/proto/distributed_query/hello.grpc.pb.h>

#include <tateyama/grpc/distributed_query/service_adapter.h>

#include <mutex>

namespace tateyama::grpc::distributed_query {

class service final : public proto::distributed_query::DistributedQuery::Service {
public:
    service() = default;

    ::grpc::Status Hello(
        ::grpc::ServerContext* context,
        proto::distributed_query::HelloRequest const* request,
        proto::distributed_query::HelloResponse* response) override;

    ::grpc::Status ExecuteRemoteProcess(
        ::grpc::ServerContext* context,
        proto::distributed_query::ExecuteRemoteProcessRequest const* request,
        ::grpc::ServerWriter<proto::distributed_query::ExecuteRemoteProcessResponse>* writer) override;

    void set_remote_process_handler(std::shared_ptr<remote_process_handler> handler);

private:
    std::mutex mutex_{};
    std::shared_ptr<remote_process_handler> handler_{};
};

} // namespace tateyama::grpc::distributed_query
