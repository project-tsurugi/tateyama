/*
 * Copyright 2026 Project Tsurugi.
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

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <tateyama/proto/distributed_query/remote_catalog.grpc.pb.h>

#include <tateyama/grpc/distributed_query/service_adapter.h>

#include "service.h"

namespace tateyama::grpc::distributed_query {

// L3-07: serialized size limit of each GetRemoteCatalog request and response.
constexpr std::size_t remote_catalog_max_message_size = std::size_t{64} * 1024U * 1024U;

/**
 * @brief splits a complete catalog into GetRemoteCatalog responses.
 * @details Each response holds a contiguous, non-empty part of the repeated fields, keeps the element order of
 * every field, and is numbered from 0. Concatenating the fragments field by field restores source, so every
 * index field keeps its meaning. An empty catalog yields no response.
 * @param source the complete catalog; its elements are moved into the responses
 * @param destination receives the responses only on success
 * @param max_message_size the serialized size limit of each response
 * @return OK, or RESOURCE_EXHAUSTED if a single element does not fit in one response
 */
[[nodiscard]] ::grpc::Status split_remote_catalog(
    ::tateyama::grpc::proto::RemoteCatalog& source,
    std::vector<::tateyama::grpc::proto::RemoteCatalogResponse>& destination,
    std::size_t max_message_size = remote_catalog_max_message_size);

class catalog_service final : public ::tateyama::grpc::proto::DistributedQueryService::Service {
public:
    catalog_service() = default;

    ::grpc::Status GetRemoteCatalog(
        ::grpc::ServerContext* context,
        ::tateyama::grpc::proto::RemoteCatalogRequest const* request,
        ::grpc::ServerWriter<::tateyama::grpc::proto::RemoteCatalogResponse>* writer) override;

    ::grpc::Status ExecuteRemoteProcess(
        ::grpc::ServerContext* context,
        ::tateyama::grpc::proto::RemoteProcessRequest const* request,
        ::grpc::ServerWriter<::tateyama::grpc::proto::RemoteProcessResult>* writer) override;

    void set_execution_service(std::shared_ptr<service> execution) { execution_ = std::move(execution); }

    // Clearing cancels and drains calls; invoke outside an executing handler.
    void set_remote_catalog_handler(std::shared_ptr<remote_catalog_handler> handler);

private:
    struct call_guard {
        call_guard(catalog_service& owner, ::grpc::ServerContext* context) : owner_(owner), context_(context) {}
        call_guard(call_guard const&) = delete;
        call_guard& operator=(call_guard const&) = delete;
        call_guard(call_guard&&) = delete;
        call_guard& operator=(call_guard&&) = delete;
        ~call_guard();
    private:
        catalog_service& owner_;
        ::grpc::ServerContext* context_;
    };
    void finish_call(::grpc::ServerContext* context);
    std::shared_ptr<service> execution_{};
    std::condition_variable idle_{};
    std::unordered_set<::grpc::ServerContext*> active_calls_{};
    std::mutex mutex_{};
    std::shared_ptr<remote_catalog_handler> handler_{};
};

} // namespace tateyama::grpc::distributed_query
