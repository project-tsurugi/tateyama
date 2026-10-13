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

#include "service.h"

#include <cstdint>
#include <exception>

#include <glog/logging.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/wire_format_lite.h>

namespace tateyama::grpc::distributed_query {

[[nodiscard]] static ::grpc::StatusCode to_grpc_status(remote_process_error_kind kind) noexcept {
    switch (kind) {
        case remote_process_error_kind::invalid_request: return ::grpc::StatusCode::INVALID_ARGUMENT;
        case remote_process_error_kind::not_found: return ::grpc::StatusCode::NOT_FOUND;
        case remote_process_error_kind::unsupported: return ::grpc::StatusCode::UNIMPLEMENTED;
        case remote_process_error_kind::unavailable: return ::grpc::StatusCode::UNAVAILABLE;
        case remote_process_error_kind::cancelled: return ::grpc::StatusCode::CANCELLED;
        case remote_process_error_kind::timeout: return ::grpc::StatusCode::DEADLINE_EXCEEDED;
        case remote_process_error_kind::internal: return ::grpc::StatusCode::INTERNAL;
        case remote_process_error_kind::resource_exhausted: return ::grpc::StatusCode::RESOURCE_EXHAUSTED;
        case remote_process_error_kind::sql_error: return ::grpc::StatusCode::UNKNOWN;
    }
    return ::grpc::StatusCode::UNKNOWN;
}

// google.rpc.Status wire contract: code=1, message=2, repeated Any details=3.
// Keep SQL/Google RPC generated types out of the Tateyama/Jogasaki API boundary.
[[nodiscard]] static std::string sql_status_details(std::string const& message, std::string const& sql_error) {
    google::protobuf::Any detail{};
    detail.set_type_url("type.googleapis.com/jogasaki.proto.sql.response.Error");
    detail.set_value(sql_error);
    std::string encoded{};
    {
        google::protobuf::io::StringOutputStream stream{&encoded};
        google::protobuf::io::CodedOutputStream output{&stream};
        using wire = google::protobuf::internal::WireFormatLite;
        wire::WriteInt32(1, static_cast<int>(::grpc::StatusCode::UNKNOWN), &output);
        wire::WriteString(2, message, &output);
        wire::WriteBytes(3, detail.SerializeAsString(), &output);
    }
    return encoded;
}

::grpc::Status service::Hello(
    ::grpc::ServerContext*,
    ::tateyama::proto::distributed_query::HelloRequest const*,
    ::tateyama::proto::distributed_query::HelloResponse*)
{
    LOG(INFO) << "hello";
    return ::grpc::Status::OK;
}

::grpc::Status service::ExecuteRemoteProcess(
    ::grpc::ServerContext* context,
    ::tateyama::grpc::proto::RemoteProcessRequest const* request,
    ::grpc::ServerWriter<::tateyama::grpc::proto::RemoteProcessResult>* writer)
{
    constexpr std::size_t max_message_size = std::size_t{64} * 1024U * 1024U;
    if (request->ByteSizeLong() > max_message_size) {
        return {::grpc::StatusCode::RESOURCE_EXHAUSTED, "remote process request exceeds 64 MiB"};
    }
    if (!request->has_execution_block() || !request->has_metadata() ||
        !request->metadata().has_protocol_version() ||
        request->metadata().protocol_version().major() == 0 ||
        request->metadata().coordinator_job_id().empty() ||
        request->metadata().remote_execution_id().empty() ||
        request->metadata().attempt_number() == 0) {
        return {::grpc::StatusCode::INVALID_ARGUMENT, "remote process request metadata is incomplete"};
    }
    if (request->metadata().protocol_version().major() != 1) {
        return {::grpc::StatusCode::FAILED_PRECONDITION, "unsupported remote process protocol major version"};
    }
    call_guard active{*this, context};
    std::shared_ptr<remote_process_handler> handler{};
    {
        std::scoped_lock<std::mutex> lock{mutex_};
        handler = handler_;
        if (!handler) {
            return {::grpc::StatusCode::UNIMPLEMENTED, "remote process handler is not registered"};
        }
        active_calls_.insert(context);
    }
    std::uint64_t sequence{};
    bool oversized{};
    bool closed{};
    remote_process_result result{};
    try {
        result = handler->execute_records(request->execution_block().SerializeAsString(), [context] {
            return context->IsCancelled();
        }, [&](std::vector<std::string> const& records) {
            if (oversized || closed || context->IsCancelled()) return false;
            ::tateyama::grpc::proto::RemoteProcessResult response{};
            response.set_sequence_number(sequence);
            for (auto const& record : records) response.add_record_list(record);
            if (records.empty()) return true;
            if (response.ByteSizeLong() > max_message_size) {
                oversized = true;
                return false;
            }
            if (!writer->Write(response)) {
                closed = true;
                return false;
            }
            ++sequence;
            return true;
        });
    } catch (std::exception const& e) {
        return {::grpc::StatusCode::INTERNAL, e.what()};
    } catch (...) {
        return {::grpc::StatusCode::INTERNAL, "remote process handler threw an unknown exception"};
    }
    if (oversized) return {::grpc::StatusCode::RESOURCE_EXHAUSTED, "remote process result exceeds 64 MiB"};
    if (closed || context->IsCancelled()) return {::grpc::StatusCode::CANCELLED, "remote process result stream was closed"};
    if (!result.success) {
        if (result.error_kind == remote_process_error_kind::sql_error && !result.sql_error_details.empty()) {
            auto details = sql_status_details(result.error, result.sql_error_details);
            return {::grpc::StatusCode::UNKNOWN, result.error, details};
        }
        return {to_grpc_status(result.error_kind), result.error};
    }
    return ::grpc::Status::OK;
}

void service::set_remote_process_handler(std::shared_ptr<remote_process_handler> handler) {
    std::unique_lock<std::mutex> lock{mutex_};
    handler_ = std::move(handler);
    if (!handler_) {
        for (auto* context : active_calls_) context->TryCancel();
        idle_.wait(lock, [this] { return active_calls_.empty(); });
    }
}

service::call_guard::~call_guard() {
    owner_.finish_call(context_);
}

void service::finish_call(::grpc::ServerContext* context) {
    std::scoped_lock<std::mutex> lock{mutex_};
    active_calls_.erase(context);
    idle_.notify_all();
}

} // namespace tateyama::grpc::distributed_query
