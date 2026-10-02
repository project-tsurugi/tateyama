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

#include "service.h"

#include <glog/logging.h>

namespace tateyama::grpc::distributed_query {

namespace {

[[nodiscard]] ::grpc::StatusCode to_grpc_status(remote_process_error_kind kind) noexcept {
    switch (kind) {
        case remote_process_error_kind::invalid_request: return ::grpc::StatusCode::INVALID_ARGUMENT;
        case remote_process_error_kind::not_found: return ::grpc::StatusCode::NOT_FOUND;
        case remote_process_error_kind::unsupported: return ::grpc::StatusCode::UNIMPLEMENTED;
        case remote_process_error_kind::unavailable: return ::grpc::StatusCode::UNAVAILABLE;
        case remote_process_error_kind::cancelled: return ::grpc::StatusCode::CANCELLED;
        case remote_process_error_kind::timeout: return ::grpc::StatusCode::DEADLINE_EXCEEDED;
        case remote_process_error_kind::internal: return ::grpc::StatusCode::INTERNAL;
    }
    return ::grpc::StatusCode::UNKNOWN;
}

} // namespace

::grpc::Status service::Hello(
    ::grpc::ServerContext*,
    proto::distributed_query::HelloRequest const*,
    proto::distributed_query::HelloResponse*)
{
    LOG(INFO) << "hello";
    return ::grpc::Status::OK;
}

::grpc::Status service::ExecuteRemoteProcess(
    ::grpc::ServerContext* context,
    proto::distributed_query::ExecuteRemoteProcessRequest const* request,
    ::grpc::ServerWriter<proto::distributed_query::ExecuteRemoteProcessResponse>* writer)
{
    if (!request->has_metadata() ||
        !request->metadata().has_protocol_version() ||
        request->metadata().coordinator_job_id().empty() ||
        request->metadata().remote_execution_id().empty() ||
        request->metadata().attempt_number() == 0) {
        return {::grpc::StatusCode::INVALID_ARGUMENT, "remote process request metadata is incomplete"};
    }
    if (request->metadata().protocol_version().major() != 1) {
        return {::grpc::StatusCode::FAILED_PRECONDITION, "unsupported remote process protocol major version"};
    }
    std::shared_ptr<remote_process_handler> handler{};
    {
        std::lock_guard<std::mutex> lock{mutex_};
        handler = handler_;
    }
    if (!handler) {
        return {::grpc::StatusCode::UNIMPLEMENTED, "remote process handler is not registered"};
    }
    auto result = handler->execute(request->payload(), [context] {
        return context->IsCancelled();
    });
    if (!result.success) {
        return {to_grpc_status(result.error_kind), std::move(result.error)};
    }
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    response.set_sequence_number(0);
    response.set_payload(std::move(result.payload));
    if (!writer->Write(response)) {
        return {::grpc::StatusCode::CANCELLED, "remote process result stream was closed"};
    }
    return ::grpc::Status::OK;
}

void service::set_remote_process_handler(std::shared_ptr<remote_process_handler> handler) {
    std::lock_guard<std::mutex> lock{mutex_};
    handler_ = std::move(handler);
}

} // namespace tateyama::grpc::distributed_query
