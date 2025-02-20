/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>

#include <tateyama/framework/repository.h>

#include <tateyama/status/resource/bridge.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/session/service/bridge.h>
#include <tateyama/session/resource/bridge.h>
#include <tateyama/metrics/service/bridge.h>
#include <tateyama/metrics/resource/bridge.h>
#include <tateyama/endpoint/ipc/bootstrap/ipc_endpoint.h>
#include <tateyama/service/mock_service.h>
#include <tateyama/faulty_service/mock_faulty_service.h>

#include "glog_helper.h"
#include "mock_server.h"

DEFINE_bool(fault, false, "fault injection");

namespace tateyama::server {

static constexpr std::string_view default_configuration_for_mock_server {  // NOLINT
    "[sql]\n"
        "thread_pool_size=5\n"
        "enable_index_join=false\n"
        "stealing_enabled=true\n"
        "default_partitions=5\n"
        "stealing_wait=1\n"
        "task_polling_wait=0\n"
        "lightweight_job_level=0\n"
        "enable_hybrid_scheduler=true\n"

    "[ipc_endpoint]\n"
        "database_name=tsurugi\n"
        "threads=104\n"
        "datachannel_buffer_size=64\n"
        "max_datachannel_buffers=16\n"
        "admin_sessions=1\n"

    "[stream_endpoint]\n"
        "port=12345\n"
        "threads=104\n"

    "[session]\n"
        "enable_timeout=false\n"
        "refresh_timeout=300\n"
        "max_refresh_timeout=86400\n"

    "[fdw]\n"
        "name=tsurugi\n"
        "threads=104\n"

    "[datastore]\n"
        "log_location=\n"
        "logging_max_parallelism=112\n"

    "[cc]\n"
        "epoch_duration=40000\n"

    "[system]\n"
        "pid_directory = /tmp\n"

    "[glog]\n"
        "dummy=\n"  // just for retain glog section in default configuration
};

static bool shutdown_requested{};

static void signal_handler([[maybe_unused]] int sig) {
    std::cout << std::endl << "catch " << strsignal(sig) << " signal" << std::endl;
    shutdown_requested = true;
}

int
mock_server_main(int argc, char **argv) {
    // command arguments
    gflags::SetUsageMessage("tateyama mock server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (signal(SIGINT, signal_handler) == SIG_ERR) {  // NOLINT  #define SIG_ERR  ((__sighandler_t) -1) in a system header file
        std::cerr << "cannot register signal handler" << std::endl;
        return 1;
    }
    if (signal(SIGQUIT, signal_handler) == SIG_ERR) {  // NOLINT  #define SIG_ERR  ((__sighandler_t) -1) in a system header file
        std::cerr << "cannot register signal handler" << std::endl;
        return 1;
    }

    // configuration
    std::stringstream ss{};
    ss << "[ipc_endpoint]\n";
    ss << "database_name=mock\n";
    ss << "[session]\n";
    ss << "enable_timeout=true\n";
    ss << "refresh_timeout=121\n";
    ss << "[glog]\n";
    ss << "logtostderr=false\n";
    auto conf = std::make_shared<tateyama::api::configuration::whole>(ss, default_configuration_for_mock_server);
    setup_glog(conf.get());

    tateyama::server::mock_server tgsv{framework::boot_mode::database_server, conf};
    tateyama::server::add_core_components(tgsv);
    
    // status_info
    auto status_info = tgsv.find_resource<tateyama::status_info::resource::bridge>();
    if (auto fd = open("/tmp/mock_server", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); fd >= 0) {
        if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
            std::cerr << "can't lock the lock_file" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "can't create the lock_file" << std::endl;
        return 1;
    }

    if (!tgsv.setup()) {
        status_info->whole(tateyama::status_info::state::boot_error);
        return 1;
    }
    // should do after setup()
    status_info->mutex_file("/tmp/mock_server");

    status_info->whole(tateyama::status_info::state::ready);

    if (!tgsv.start()) {
        status_info->whole(tateyama::status_info::state::boot_error);
        // detailed message must have been logged in the components where start error occurs
        LOG(ERROR) << "Starting server failed due to errors in starting server application framework.";
        tgsv.shutdown();
        return 1;
    }

    status_info->whole(tateyama::status_info::state::activated);

    // wait for a shutdown request
    while(!shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // termination process
    status_info->whole(tateyama::status_info::state::deactivating);
    tgsv.shutdown();

    std::remove("/tmp/mock_server");
    return 0;
}


void mock_server::add_resource(std::shared_ptr<resource> arg) {
    environment_->resource_repository().add(std::move(arg));
}

void mock_server::add_service(std::shared_ptr<service> arg) {
    environment_->service_repository().add(std::move(arg));
}

void mock_server::add_endpoint(std::shared_ptr<endpoint> arg) {
    environment_->endpoint_repository().add(std::move(arg));
}

bool mock_server::setup() {
    if(setup_done_) return true;
    bool success = true;
    environment_->resource_repository().each([this, &success](resource& arg){
        if (! success) return;
        success = arg.setup(*environment_);
    });
    environment_->service_repository().each([this, &success](service& arg){
        if (! success) return;
        success = arg.setup(*environment_);
    });
    environment_->endpoint_repository().each([this, &success](endpoint& arg){
        if (! success) return;
        success = arg.setup(*environment_);
    });
    if(! success) {
        LOG(ERROR) << "Server application framework setup phase failed.";
        // shutdown already setup components
        shutdown();
    }
    setup_done_ = success;
    return success;
}

bool mock_server::start() {
    if(! setup_done_) {
        if(! setup()) {
            // error logged in setup() already
            return false;
        }
    }
    bool success = true;
    environment_->resource_repository().each([this, &success](resource& arg){
        if (! success) return;
        success = arg.start(*environment_);
    });
    environment_->service_repository().each([this, &success](service& arg){
        if (! success) return;
        success = arg.start(*environment_);
    });
    environment_->endpoint_repository().each([this, &success](endpoint& arg){
        if (! success) return;
        success = arg.start(*environment_);
    });
    if(! success) {
        LOG(ERROR) << "Server application framework start phase failed.";
        // shutdown already started components
        shutdown();
    }
    return success;
}

bool mock_server::shutdown() {
    // even if some components fails, continue all shutdown for clean-up
    bool success = true;
    environment_->endpoint_repository().each([this, &success](endpoint& arg){
        success = arg.shutdown(*environment_) && success;
    }, true);
    environment_->service_repository().each([this, &success](service& arg){
        success = arg.shutdown(*environment_) && success;
    }, true);
    environment_->resource_repository().each([this, &success](resource& arg){
        success = arg.shutdown(*environment_) && success;
    }, true);
    return success;
}

mock_server::mock_server(framework::boot_mode mode, std::shared_ptr<api::configuration::whole> cfg) :
    environment_(std::make_shared<environment>(mode, std::move(cfg))) {
}

void add_core_components(mock_server& svr) {
    svr.add_resource(std::make_shared<metrics::resource::bridge>());
    svr.add_resource(std::make_shared<status_info::resource::bridge>());
    svr.add_resource(std::make_shared<session::resource::bridge>());

    svr.add_service(std::make_shared<framework::routing_service>());
    svr.add_service(std::make_shared<metrics::service::bridge>());
    if (FLAGS_fault) {
        std::cout << "mock_server with fault injection" << std::endl;
        svr.add_service(std::make_shared<::tateyama::service::mock_faulty_service>());
    } else {
        std::cout << "mock_server for blob/clob test" << std::endl;
        svr.add_service(std::make_shared<::tateyama::service::mock_service>());
    }
    svr.add_service(std::make_shared<session::service::bridge>());

    svr.add_endpoint(std::make_shared<framework::ipc_endpoint>());
}

} // namespace tateyama::server


int
main(int argc, char **argv) {
    return tateyama::server::mock_server_main(argc, argv);
}
