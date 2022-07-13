/*
 * Copyright 2018-2020 tsurugi project.
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
#include <tateyama/api/environment.h>

#include <tateyama/api/server/service.h>
#include <tateyama/api/endpoint/provider.h>
#include <tateyama/api/endpoint/service.h>
#include <tateyama/api/configuration.h>

namespace tateyama::api {

using takatori::util::sequence_view;

void environment::add_application(std::shared_ptr<server::service> app) {
    applications_.emplace_back(std::move(app));
}

sequence_view<std::shared_ptr<server::service> const> environment::applications() const noexcept {
    return applications_;
}

void environment::add_endpoint(std::shared_ptr<endpoint::provider> endpoint) {
    endpoints_.emplace_back(std::move(endpoint));
}

sequence_view<std::shared_ptr<endpoint::provider> const> environment::endpoints() const noexcept {
    return endpoints_;
}

void environment::endpoint_service(std::shared_ptr<endpoint::service> svc) noexcept {
    service_ = std::move(svc);
}

std::shared_ptr<endpoint::service> const& environment::endpoint_service() const noexcept {
    return service_;
}

void environment::configuration(std::shared_ptr<configuration::whole> svc) noexcept {
    configuration_ = std::move(svc);
}

std::shared_ptr<configuration::whole> const& environment::configuration() const noexcept {
    return configuration_;
}
}
