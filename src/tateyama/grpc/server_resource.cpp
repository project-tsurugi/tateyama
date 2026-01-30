/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include <tateyama/grpc/server_resource.h>
#include "server_resource_impl.h"

namespace tateyama::grpc {

using namespace framework;

grpc_server_resource::grpc_server_resource()
    : impl_(std::unique_ptr<resource_impl, void(*)(resource_impl*)>(new resource_impl(), [](resource_impl* e){ delete e; })) {  // NOLINT(cppcoreguidelines-owning-memory)
}

bool grpc_server_resource::setup(framework::environment& env) {
    return impl_->setup(env);
}

bool grpc_server_resource::start(framework::environment& env) {
    return impl_->start(env);
}

bool grpc_server_resource::shutdown(framework::environment& env) {
    return impl_->shutdown(env);
}

framework::component::id_type grpc_server_resource::id() const noexcept {
    return tag;
}

std::string_view grpc_server_resource::label() const noexcept {
    return component_label;
}

void grpc_server_resource::add_service(::grpc::Service* service) {
    impl_->add_service(service);
}

}
