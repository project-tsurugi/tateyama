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
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/task_scheduler_resource.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/endpoint/ipc/bootstrap/ipc_endpoint.h>
#include <tateyama/endpoint/stream/bootstrap/stream_endpoint.h>
#include <tateyama/datastore/datastore_service.h>
#include <tateyama/datastore/datastore_resource.h>

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
    environment_->resource_repository().each([this](resource& arg){
        arg.setup(*environment_);
    });
    environment_->service_repository().each([this](service& arg){
        arg.setup(*environment_);
    });
    environment_->endpoint_repository().each([this](endpoint& arg){
        arg.setup(*environment_);
    });

    environment_->resource_repository().each([this](resource& arg){
        arg.start(*environment_);
    });
    environment_->service_repository().each([this](service& arg){
        arg.start(*environment_);
    });
    environment_->endpoint_repository().each([this](endpoint& arg){
        arg.start(*environment_);
    });
}

void server::shutdown() {
    environment_->endpoint_repository().each([this](endpoint& arg){
        arg.shutdown(*environment_);
    });
    environment_->service_repository().each([this](service& arg){
        arg.shutdown(*environment_);
    });
    environment_->resource_repository().each([this](resource& arg){
        arg.shutdown(*environment_);
    });
}

std::shared_ptr<service> server::find_service_by_id(component::id_type id) {
    return environment_->service_repository().find_by_id(id);
}

void install_core_components(server& svr) {
    svr.add_resource(std::make_shared<framework::transactional_kvs_resource>());
    svr.add_resource(std::make_shared<datastore::datastore_resource>());

    svr.add_service(std::make_shared<framework::routing_service>());
    svr.add_service(std::make_shared<datastore::datastore_service>());

    svr.add_endpoint(std::make_shared<framework::ipc_endpoint>());
    svr.add_endpoint(std::make_shared<framework::stream_endpoint>());
}

}
