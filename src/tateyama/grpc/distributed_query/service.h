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

#include <grpcpp/grpcpp.h>

#include <tateyama/proto/distributed_query/hello.grpc.pb.h>
#include <tateyama/proto/distributed_query/remote_catalog.grpc.pb.h>

#include <tateyama/grpc/distributed_query/service_adapter.h>

#include <condition_variable>
#include <mutex>
#include <unordered_set>

namespace tateyama::grpc::distributed_query {

class service final : public ::tateyama::proto::distributed_query::DistributedQuery::Service {
public:
    service() = default;

    ::grpc::Status Hello(
        ::grpc::ServerContext* context,
        ::tateyama::proto::distributed_query::HelloRequest const* request,
        ::tateyama::proto::distributed_query::HelloResponse* response) override;

    ::grpc::Status ExecuteRemoteProcess(
        ::grpc::ServerContext* context,
        ::tateyama::grpc::proto::RemoteProcessRequest const* request,
        ::grpc::ServerWriter<::tateyama::grpc::proto::RemoteProcessResult>* writer);

    // Clearing the handler cancels and waits for active calls before returning.
    // Must be called outside an executing handler.
    void set_remote_process_handler(std::shared_ptr<remote_process_handler> handler);

private:
    struct call_guard {
        call_guard(service& owner, ::grpc::ServerContext* context) : owner_(owner), context_(context) {}
        call_guard(call_guard const&) = delete;
        call_guard& operator=(call_guard const&) = delete;
        call_guard(call_guard&&) = delete;
        call_guard& operator=(call_guard&&) = delete;
        ~call_guard();
    private:
        service& owner_;
        ::grpc::ServerContext* context_;
    };
    void finish_call(::grpc::ServerContext* context);
    std::condition_variable idle_{};
    std::unordered_set<::grpc::ServerContext*> active_calls_{};
    std::mutex mutex_{};
    std::shared_ptr<remote_process_handler> handler_{};
};

} // namespace tateyama::grpc::distributed_query
