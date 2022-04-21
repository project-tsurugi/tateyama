/*
 * Copyright 2018-2022 tsurugi project.
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

#include <tateyama/framework/repository.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/api/configuration.h>
#include <tateyama/framework/boot_mode.h>

namespace tateyama::framework {

boot_mode tateyama::framework::environment::mode() {
    return mode_;
}

std::shared_ptr<api::configuration::whole> const& environment::configuration() {
    return configuration_;
}

repository<resource>& environment::resource_repository() {
    return resource_repository_;
}

repository<service>& environment::service_repository() {
    return service_repository_;
}

repository<endpoint>& environment::endpoint_repository() {
    return endpoint_repository_;
}

environment::environment(boot_mode mode, std::shared_ptr<api::configuration::whole> cfg) :
    mode_(mode),
    configuration_(std::move(cfg))
{}

}

