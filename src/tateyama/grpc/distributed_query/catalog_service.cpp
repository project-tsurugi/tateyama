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

#include "catalog_service.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>

#include <glog/logging.h>
#include <google/protobuf/io/coded_stream.h>

namespace tateyama::grpc::distributed_query {

namespace {

namespace wire = ::tateyama::grpc::proto;

/**
 * @brief packs catalog elements into responses, keeping the serialized size of each response within the limit.
 * @details Sizes are computed from the wire format instead of serializing every candidate response.
 */
class fragment_builder {
public:
    explicit fragment_builder(std::size_t limit) noexcept : limit_(limit) {}

    // false if the element alone exceeds the limit
    template <class Element, class Add>
    [[nodiscard]] bool place(Element& element, Add add) {
        auto element_size = element.ByteSizeLong();
        // RemoteCatalog field numbers 1..6 have one-byte tags
        auto entry = 1U + coded::VarintSize64(element_size) + element_size;
        if (!empty_ && response_size(fragment_size_ + entry) > limit_) {
            flush();
        }
        if (response_size(fragment_size_ + entry) > limit_) {
            return false;
        }
        add(current_.mutable_catalog_fragment())->Swap(&element);
        fragment_size_ += entry;
        empty_ = false;
        return true;
    }

    [[nodiscard]] std::vector<wire::RemoteCatalogResponse> finish() {
        if (!empty_) {
            flush();
        }
        return std::move(result_);
    }

private:
    using coded = google::protobuf::io::CodedOutputStream;

    std::size_t limit_;
    std::vector<wire::RemoteCatalogResponse> result_{};
    wire::RemoteCatalogResponse current_{};
    std::size_t fragment_size_{};
    bool empty_{true};

    // size of the response carrying the next sequence number and a fragment of the given size
    [[nodiscard]] std::size_t response_size(std::size_t fragment_size) const noexcept {
        auto sequence = static_cast<std::uint64_t>(result_.size());
        std::size_t size = 1U + coded::VarintSize64(fragment_size) + fragment_size;
        if (sequence != 0) {
            size += 1U + coded::VarintSize64(sequence);
        }
        return size;
    }

    void flush() {
        current_.set_sequence_number(static_cast<std::uint64_t>(result_.size()));
        result_.emplace_back(std::move(current_));
        current_ = {};
        fragment_size_ = 0;
        empty_ = true;
    }
};

} // namespace

template <class Field, class Add>
[[nodiscard]] static bool place_all(fragment_builder& builder, Field& field, Add add) {
    for (auto& element : field) {
        if (!builder.place(element, add)) {
            return false;
        }
    }
    return true;
}



::grpc::Status split_remote_catalog(
    wire::RemoteCatalog& source,
    std::vector<wire::RemoteCatalogResponse>& destination,
    std::size_t max_message_size)
{
    fragment_builder builder{max_message_size};
    bool placed =
        place_all(builder, *source.mutable_schema_list(), [](auto* c) { return c->add_schema_list(); }) &&
        place_all(builder, *source.mutable_table_list(), [](auto* c) { return c->add_table_list(); }) &&
        place_all(builder, *source.mutable_column_list(), [](auto* c) { return c->add_column_list(); }) &&
        place_all(builder, *source.mutable_index_list(), [](auto* c) { return c->add_index_list(); }) &&
        place_all(builder, *source.mutable_function_list(), [](auto* c) { return c->add_function_list(); }) &&
        place_all(builder, *source.mutable_aggregate_function_list(),
            [](auto* c) { return c->add_aggregate_function_list(); });
    if (!placed) {
        return {::grpc::StatusCode::RESOURCE_EXHAUSTED, "a remote catalog element exceeds the 64 MiB response limit"};
    }
    auto responses = builder.finish();
    for (auto const& response : responses) {
        if (response.ByteSizeLong() > max_message_size) {
            return {::grpc::StatusCode::INTERNAL, "remote catalog fragment exceeds the computed size"};
        }
    }
    destination = std::move(responses);
    return ::grpc::Status::OK;
}

::grpc::Status catalog_service::ExecuteRemoteProcess(
    ::grpc::ServerContext* context,
    ::tateyama::grpc::proto::RemoteProcessRequest const* request,
    ::grpc::ServerWriter<::tateyama::grpc::proto::RemoteProcessResult>* writer) {
    if (!execution_) return {::grpc::StatusCode::UNIMPLEMENTED, "remote execution is not registered"};
    return execution_->ExecuteRemoteProcess(context, request, writer);
}

::grpc::Status catalog_service::GetRemoteCatalog(
    ::grpc::ServerContext* context,
    wire::RemoteCatalogRequest const* request,
    ::grpc::ServerWriter<wire::RemoteCatalogResponse>* writer)
{
    auto started = std::chrono::steady_clock::now();
    std::size_t fragments_sent{};
    std::size_t bytes_sent{};
    std::string counts{};
    auto finish = [&](::grpc::Status status) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
        auto const& metadata = request->metadata();
        LOG(INFO) << "GetRemoteCatalog request_id:" << metadata.request_id()
                  << " version:" << metadata.protocol_version().major() << "." << metadata.protocol_version().minor()
                  << " peer:" << context->peer() << " status:" << status.error_code()
                  << " message:" << status.error_message() << counts << " fragments:" << fragments_sent
                  << " request_bytes:" << request->ByteSizeLong() << " response_bytes:" << bytes_sent
                  << " elapsed_ms:" << elapsed.count();
        return status;
    };
    if (request->ByteSizeLong() > remote_catalog_max_message_size) {
        return finish({::grpc::StatusCode::RESOURCE_EXHAUSTED, "remote catalog request exceeds 64 MiB"});
    }
    if (!request->has_metadata() ||
        !request->metadata().has_protocol_version() ||
        request->metadata().request_id().empty()) {
        return finish({::grpc::StatusCode::INVALID_ARGUMENT, "remote catalog request metadata is incomplete"});
    }
    if (request->metadata().protocol_version().major() != 1) {
        return finish({::grpc::StatusCode::FAILED_PRECONDITION, "unsupported remote catalog protocol major version"});
    }
    call_guard active{*this, context};
    std::shared_ptr<remote_catalog_handler> handler{};
    {
        std::scoped_lock<std::mutex> lock{mutex_};
        handler = handler_;
        if (!handler) {
            return finish({::grpc::StatusCode::UNIMPLEMENTED, "remote catalog handler is not registered"});
        }
        active_calls_.insert(context);
    }
    // No response is written until the complete catalog has been built and split.
    remote_catalog_result result{};
    wire::RemoteCatalog catalog{};
    std::vector<wire::RemoteCatalogResponse> responses{};
    try {
        result = (*handler)();
        if (!result.success) {
            auto code = result.error_kind == remote_catalog_error_kind::unsupported ?
                ::grpc::StatusCode::UNIMPLEMENTED : ::grpc::StatusCode::INTERNAL;
            return finish({code, result.error});
        }
        if (!catalog.ParseFromString(result.catalog)) {
            return finish({::grpc::StatusCode::INTERNAL, "remote catalog handler returned a malformed catalog"});
        }
        result.catalog.clear();
        counts = " schemas:" + std::to_string(catalog.schema_list_size()) +
            " tables:" + std::to_string(catalog.table_list_size()) +
            " columns:" + std::to_string(catalog.column_list_size()) +
            " indices:" + std::to_string(catalog.index_list_size()) +
            " functions:" + std::to_string(catalog.function_list_size()) +
            " aggregate_functions:" + std::to_string(catalog.aggregate_function_list_size());
        if (auto split = split_remote_catalog(catalog, responses); !split.ok()) {
            return finish(split);
        }
    } catch (std::exception const& e) {
        return finish({::grpc::StatusCode::INTERNAL, e.what()});
    } catch (...) {
        return finish({::grpc::StatusCode::INTERNAL, "remote catalog handler threw an unknown exception"});
    }
    for (auto const& response : responses) {
        if (context->IsCancelled()) {
            return finish({::grpc::StatusCode::CANCELLED, "remote catalog request was cancelled"});
        }
        auto size = response.ByteSizeLong();
        if (!writer->Write(response)) {
            return finish({::grpc::StatusCode::CANCELLED, "remote catalog stream was closed"});
        }
        ++fragments_sent;
        bytes_sent += size;
    }
    return finish(::grpc::Status::OK);
}

void catalog_service::set_remote_catalog_handler(std::shared_ptr<remote_catalog_handler> handler) {
    std::unique_lock<std::mutex> lock{mutex_};
    handler_ = std::move(handler);
    if (!handler_) {
        for (auto* context : active_calls_) context->TryCancel();
        idle_.wait(lock, [this] { return active_calls_.empty(); });
    }
}

catalog_service::call_guard::~call_guard() {
    owner_.finish_call(context_);
}

void catalog_service::finish_call(::grpc::ServerContext* context) {
    std::scoped_lock<std::mutex> lock{mutex_};
    active_calls_.erase(context);
    idle_.notify_all();
}

} // namespace tateyama::grpc::distributed_query
