/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <tateyama/logging.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/boot_mode.h>
#include <tateyama/api/configuration.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/endpoint/ipc/bootstrap/ipc_endpoint.h>
#include <tateyama/endpoint/stream/bootstrap/stream_endpoint.h>
#include <tateyama/datastore/service/bridge.h>
#include <tateyama/datastore/resource/bridge.h>
#include <tateyama/debug/service.h>
#include <tateyama/status/resource/bridge.h>
#include <tateyama/diagnostic/resource/diagnostic_resource.h>
#include <tateyama/utils/boolalpha.h>

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

bool server::setup() {
    if(setup_done_) return true;
    bool success = true;
    environment_->resource_repository().each([this, &success](resource& arg){
        if (! success) return;
        VLOG(log_info) << "/:tateyama:lifecycle:component:setup_begin " << arg.label();
        success = arg.setup(*environment_);
        VLOG(log_info) << "/:tateyama:lifecycle:component:setup_end " << arg.label() << " success:" << utils::boolalpha(success);
    });
    environment_->service_repository().each([this, &success](service& arg){
        if (! success) return;
        VLOG(log_info) << "/:tateyama:lifecycle:component:setup_begin " << arg.label();
        success = arg.setup(*environment_);
        VLOG(log_info) << "/:tateyama:lifecycle:component:setup_end " << arg.label() << " success:" << utils::boolalpha(success);
    });
    environment_->endpoint_repository().each([this, &success](endpoint& arg){
        if (! success) return;
        VLOG(log_info) << "/:tateyama:lifecycle:component:setup_begin " << arg.label();
        success = arg.setup(*environment_);
        VLOG(log_info) << "/:tateyama:lifecycle:component:setup_end " << arg.label() << " success:" << utils::boolalpha(success);
    });
    if(! success) {
        LOG(ERROR) << "Server application framework setup phase failed.";
        // shutdown already setup components
        shutdown();
    }
    setup_done_ = success;
    return success;
}

bool server::start() {
    if(! setup_done_) {
        if(! setup()) {
            // error logged in setup() already
            return false;
        }
    }
    bool success = true;
    environment_->resource_repository().each([this, &success](resource& arg){
        if (! success) return;
        VLOG(log_info) << "/:tateyama:lifecycle:component:start_begin " << arg.label();
        success = arg.start(*environment_);
        VLOG(log_info) << "/:tateyama:lifecycle:component:start_end " << arg.label() << " success:" << utils::boolalpha(success);
    });
    environment_->service_repository().each([this, &success](service& arg){
        if (! success) return;
        VLOG(log_info) << "/:tateyama:lifecycle:component:start_begin " << arg.label();
        success = arg.start(*environment_);
        VLOG(log_info) << "/:tateyama:lifecycle:component:start_end " << arg.label() << " success:" << utils::boolalpha(success);
    });
    environment_->endpoint_repository().each([this, &success](endpoint& arg){
        if (! success) return;
        VLOG(log_info) << "/:tateyama:lifecycle:component:start_begin " << arg.label();
        success = arg.start(*environment_);
        VLOG(log_info) << "/:tateyama:lifecycle:component:start_end " << arg.label() << " success:" << utils::boolalpha(success);
    });
    if(! success) {
        LOG(ERROR) << "Server application framework start phase failed.";
        // shutdown already started components
        shutdown();
    }
    return success;
}

bool server::shutdown() {
    // even if some components fails, continue all shutdown for clean-up
    bool success = true;
    environment_->endpoint_repository().each([this, &success](endpoint& arg){
        VLOG(log_info) << "/:tateyama:lifecycle:component:shutdown_begin " << arg.label();
        success = arg.shutdown(*environment_) && success;
        VLOG(log_info) << "/:tateyama:lifecycle:component:shutdown_end " << arg.label();
    }, true);
    environment_->service_repository().each([this, &success](service& arg){
        VLOG(log_info) << "/:tateyama:lifecycle:component:shutdown_begin " << arg.label();
        success = arg.shutdown(*environment_) && success;
        VLOG(log_info) << "/:tateyama:lifecycle:component:shutdown_end " << arg.label();
    }, true);
    environment_->resource_repository().each([this, &success](resource& arg){
        VLOG(log_info) << "/:tateyama:lifecycle:component:shutdown_begin " << arg.label();
        success = arg.shutdown(*environment_) && success;
        VLOG(log_info) << "/:tateyama:lifecycle:component:shutdown_end " << arg.label();
    }, true);
    return success;
}

std::shared_ptr<service> server::find_service_by_id(component::id_type id) {
    return environment_->service_repository().find_by_id(id);
}

server::server(framework::boot_mode mode, std::shared_ptr<api::configuration::whole> cfg) :
    environment_(std::make_shared<environment>(mode, std::move(cfg)))
{}

void add_core_components(server& svr) {
    svr.add_resource(std::make_shared<diagnostic::resource::diagnostic_resource>());
    svr.add_resource(std::make_shared<status_info::resource::bridge>());
    svr.add_resource(std::make_shared<framework::transactional_kvs_resource>());
    svr.add_resource(std::make_shared<datastore::resource::bridge>());
    svr.add_resource(std::make_shared<datastore::resource::bridge>());

    svr.add_service(std::make_shared<framework::routing_service>());
    svr.add_service(std::make_shared<datastore::service::bridge>());
    svr.add_service(std::make_shared<debug::service>());

    svr.add_endpoint(std::make_shared<framework::ipc_endpoint>());
    svr.add_endpoint(std::make_shared<framework::stream_endpoint>());
}

}

