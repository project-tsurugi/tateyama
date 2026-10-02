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

#include <memory>

#include <tateyama/framework/environment.h>

#include "service.h"

namespace tateyama::grpc::distributed_query {

class service_adapter_impl {
public:
    service_adapter_impl();
    ~service_adapter_impl();

    service_adapter_impl(service_adapter_impl const& other) = delete;
    service_adapter_impl& operator=(service_adapter_impl const& other) = delete;
    service_adapter_impl(service_adapter_impl&& other) noexcept = delete;
    service_adapter_impl& operator=(service_adapter_impl&& other) noexcept = delete;

    bool setup(framework::environment& env);

    void set_remote_process_handler(std::shared_ptr<remote_process_handler> handler);

private:
    std::shared_ptr<service> service_{};
    std::shared_ptr<remote_process_handler> handler_{};
};

} // namespace tateyama::grpc::distributed_query
