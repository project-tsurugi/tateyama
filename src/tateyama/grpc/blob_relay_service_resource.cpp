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

#include <tateyama/grpc/blob_relay_service_resource.h>
#include "blob_relay_service_resource_impl.h"

namespace tateyama::grpc {

using namespace framework;

blob_relay_service_resource::blob_relay_service_resource()
    : impl_(std::unique_ptr<resource_impl, void(*)(resource_impl*)>(new resource_impl(), [](resource_impl* e){ delete e; })) {  // NOLINT
}

blob_relay_service_resource::~blob_relay_service_resource() = default;

std::shared_ptr<blob_session> blob_relay_service_resource::create_session(std::optional<blob_relay_service_resource::transaction_id_type> transaction_id) {
    return impl_->create_session(transaction_id);
}

component::id_type blob_relay_service_resource::id() const noexcept {
    return tag;
}

bool blob_relay_service_resource::setup(environment& env) {
    return impl_->setup(env);
}

bool blob_relay_service_resource::start(environment& env) {
    return impl_->start(env);
}

bool blob_relay_service_resource::shutdown(environment& env) {
    return impl_->shutdown(env);
}

std::string_view blob_relay_service_resource::label() const noexcept {
    return component_label;
}

}
