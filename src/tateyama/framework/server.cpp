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
#include <tateyama/framework/server.h>

#include <functional>
#include <memory>
#include <type_traits>

#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/boot_mode.h>
#include <tateyama/api/configuration.h>

namespace tateyama::framework {

void server::add_resource(std::shared_ptr<resource> arg) {
    environment_->resource_repository().add(std::move(arg));
}

void server::add_service(std::shared_ptr<service> arg) {
    environment_->service_repository().add(std::move(arg));
}

void server::add_endpoint(std::shared_ptr<endpoint> arg) {
    environment_->endpoint_repository().add(std::move(arg));
}

std::shared_ptr<resource> server::find_resource_by_id(component::id_type id) {
    return environment_->resource_repository().find_by_id(id);
}

void server::start() {
    environment_->resource_repository().each([=](resource& arg){
        arg.setup(*environment_);
    });
    environment_->service_repository().each([=](service& arg){
        arg.setup(*environment_);
    });
    environment_->endpoint_repository().each([=](endpoint& arg){
        arg.setup(*environment_);
    });

    environment_->resource_repository().each([=](resource& arg){
        arg.start(*environment_);
    });
    environment_->service_repository().each([=](service& arg){
        arg.start(*environment_);
    });
    environment_->endpoint_repository().each([=](endpoint& arg){
        arg.start(*environment_);
    });
}

void server::shutdown() {
    environment_->endpoint_repository().each([=](endpoint& arg){
        arg.shutdown(*environment_);
    });
    environment_->service_repository().each([=](service& arg){
        arg.shutdown(*environment_);
    });
    environment_->resource_repository().each([=](resource& arg){
        arg.shutdown(*environment_);
    });
}

std::shared_ptr<service> server::find_service_by_id(component::id_type id) {
    return environment_->service_repository().find_by_id(id);
}
}

