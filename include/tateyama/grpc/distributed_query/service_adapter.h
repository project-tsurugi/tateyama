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

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::grpc::distributed_query {

class service_adapter_impl;

enum class remote_process_error_kind {
    invalid_request,
    not_found,
    unsupported,
    unavailable,
    cancelled,
    timeout,
    internal,
    resource_exhausted,
    sql_error,
};

struct remote_process_result {
    bool success{};
    std::string payload{};
    std::string error{};
    remote_process_error_kind error_kind{remote_process_error_kind::invalid_request};
    // Serialized jogasaki.proto.sql.response.Error, produced by the SQL engine.
    // Used only for sql_error; Tateyama transports it without depending on Jogasaki.
    std::string sql_error_details{};
};

class remote_process_handler {
public:
    using cancellation_check = std::function<bool()>;
    // The sink borrows payload only during the call. Calls are serialized by the handler.
    // A slow consumer may block the sink; cancellation must unblock the transport.
    using result_sink = std::function<bool(std::string_view)>;
    /** @brief receive complete encoded records without an engine-specific result envelope. */
    using record_sink = std::function<bool(std::vector<std::string> const&)>;

    remote_process_handler() = default;
    virtual ~remote_process_handler() = default;

    remote_process_handler(remote_process_handler const&) = default;
    remote_process_handler& operator=(remote_process_handler const&) = default;
    remote_process_handler(remote_process_handler&&) noexcept = default;
    remote_process_handler& operator=(remote_process_handler&&) noexcept = default;

    [[nodiscard]] virtual remote_process_result operator()(std::string_view payload) = 0;

    [[nodiscard]] virtual remote_process_result execute(
        std::string_view payload,
        cancellation_check const& is_cancelled)
    {
        (void) is_cancelled;
        return (*this)(payload);
    }

    /**
     * @brief execute the typed RPC, yielding complete records.
     * @details Engines with a result envelope override this method to extract encoded records.
     * The default implementation treats each payload as one complete record.
     */
    [[nodiscard]] virtual remote_process_result execute_records(
        std::string_view payload, cancellation_check const& is_cancelled, record_sink const& sink)
    {
        return execute_stream(payload, is_cancelled, [&](std::string_view record) {
            return sink(std::vector<std::string>{std::string{record}});
        });
    }

    [[nodiscard]] virtual remote_process_result execute_stream(
        std::string_view payload, cancellation_check const& is_cancelled, result_sink const& sink)
    {
        auto result = execute(payload, is_cancelled);
        if (result.success && !sink(result.payload)) {
            return {false, {}, "remote result stream was closed", remote_process_error_kind::cancelled};
        }
        result.payload.clear();
        return result;
    }
};

enum class remote_catalog_error_kind {
    // the engine found a declaration or type outside phase-1 (UNIMPLEMENTED)
    unsupported,
    // the engine failed to build the catalog (INTERNAL)
    internal,
};

struct remote_catalog_result {
    bool success{};
    // Serialized complete tateyama.grpc.proto.RemoteCatalog, produced by the SQL engine.
    // Tateyama splits it into GetRemoteCatalog fragments; the RPC itself stays typed.
    std::string catalog{};
    std::string error{};
    remote_catalog_error_kind error_kind{remote_catalog_error_kind::internal};
};

class remote_catalog_handler {
public:
    remote_catalog_handler() = default;
    virtual ~remote_catalog_handler() = default;

    remote_catalog_handler(remote_catalog_handler const&) = default;
    remote_catalog_handler& operator=(remote_catalog_handler const&) = default;
    remote_catalog_handler(remote_catalog_handler&&) noexcept = default;
    remote_catalog_handler& operator=(remote_catalog_handler&&) noexcept = default;

    // Builds the complete catalog of this database instance, once per GetRemoteCatalog request.
    [[nodiscard]] virtual remote_catalog_result operator()() = 0;
};

class service_adapter : public framework::resource {
public:
    static constexpr framework::component::id_type tag =
        framework::resource_id_distributed_query_service;
    static constexpr std::string_view component_label =
        "distributed_query_service_adapter";

    service_adapter();
    ~service_adapter() override;

    service_adapter(service_adapter const& other) = delete;
    service_adapter& operator=(service_adapter const& other) = delete;
    service_adapter(service_adapter&& other) noexcept = delete;
    service_adapter& operator=(service_adapter&& other) noexcept = delete;

    bool setup(framework::environment& env) override;
    bool start(framework::environment& env) override;
    bool shutdown(framework::environment& env) override;

    [[nodiscard]] framework::component::id_type id() const noexcept override;
    [[nodiscard]] std::string_view label() const noexcept override;

    void set_remote_process_handler(std::shared_ptr<remote_process_handler> handler);

    // GetRemoteCatalog returns UNIMPLEMENTED while no handler is set.
    void set_remote_catalog_handler(std::shared_ptr<remote_catalog_handler> handler);

private:
    std::unique_ptr<service_adapter_impl, void (*)(service_adapter_impl*)> impl_;
};

} // namespace tateyama::grpc::distributed_query
