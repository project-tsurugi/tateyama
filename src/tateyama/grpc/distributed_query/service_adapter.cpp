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

#include <tateyama/grpc/distributed_query/service_adapter.h>

#include "service_adapter_impl.h"

namespace tateyama::grpc::distributed_query {

service_adapter::service_adapter()
    : impl_(
          std::unique_ptr<service_adapter_impl, void (*)(service_adapter_impl*)>(
              new service_adapter_impl,
              [](service_adapter_impl* e) { delete e; })) // NOLINT
{}

service_adapter::~service_adapter() = default;

bool service_adapter::setup(framework::environment& env) {
    return impl_->setup(env);
}

bool service_adapter::start(framework::environment&) {
    return true;
}

bool service_adapter::shutdown(framework::environment&) {
    return true;
}

framework::component::id_type service_adapter::id() const noexcept {
    return tag;
}

std::string_view service_adapter::label() const noexcept {
    return component_label;
}

void service_adapter::set_remote_process_handler(std::shared_ptr<remote_process_handler> handler) {
    impl_->set_remote_process_handler(std::move(handler));
}

} // namespace tateyama::grpc::distributed_query
