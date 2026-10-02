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

#include <functional>
#include <memory>
#include <string>
#include <string_view>

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
};

struct remote_process_result {
    bool success{};
    std::string payload{};
    std::string error{};
    remote_process_error_kind error_kind{remote_process_error_kind::invalid_request};
};

class remote_process_handler {
public:
    using cancellation_check = std::function<bool()>;

    virtual ~remote_process_handler() = default;

    [[nodiscard]] virtual remote_process_result operator()(std::string_view payload) = 0;

    [[nodiscard]] virtual remote_process_result execute(
        std::string_view payload,
        cancellation_check const& is_cancelled)
    {
        (void) is_cancelled;
        return (*this)(payload);
    }
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

private:
    std::unique_ptr<service_adapter_impl, void (*)(service_adapter_impl*)> impl_;
};

} // namespace tateyama::grpc::distributed_query
