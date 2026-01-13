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

#include <tateyama/framework/environment.h>

#include <tateyama/grpc/blob_relay/service_proxy.h>
#include "service_proxy_impl.h"

namespace tateyama::grpc::blob_relay {

using namespace framework;

std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> service_proxy::blob_relay_service() {
    return impl_->blob_relay_service();
}

service_proxy::service_proxy()
    : impl_(std::unique_ptr<service_proxy_impl, void(*)(service_proxy_impl*)>(new service_proxy_impl, [](service_proxy_impl* e){delete e;})) {  // NOLINT(cppcoreguidelines-owning-memory)
}

service_proxy::~service_proxy() = default;

bool service_proxy::setup(environment& env) {
    return impl_->setup(env);
}

bool service_proxy::start(environment&) {
    return true;
}

bool service_proxy::shutdown(environment&) {
    return true;
}

component::id_type service_proxy::id() const noexcept {
    return tag;
}

std::string_view service_proxy::label() const noexcept {
    return component_label;
}

}
