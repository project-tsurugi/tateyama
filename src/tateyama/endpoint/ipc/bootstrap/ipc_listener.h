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
#pragma once

#include <memory>
#include <string>
#include <exception>
#include <chrono>
#include <csignal>

#include <boost/thread/barrier.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/api/configuration.h>

#include "tateyama/endpoint/common/logging.h"
#include "worker.h"

namespace tateyama::server {

/**
 * @brief ipc listener
 */
class ipc_listener {
public:
    explicit ipc_listener(const std::shared_ptr<api::configuration::whole>& cfg, std::shared_ptr<framework::routing_service> router, std::shared_ptr<status_info::resource::bridge> status) :
        cfg_(cfg), router_(std::move(router)), status_(std::move(status))
    {
        auto endpoint_config = cfg->get_section("ipc_endpoint");
        if (endpoint_config == nullptr) {
            throw std::runtime_error("cannot find ipc_endpoint section in the configuration");
        }

        auto database_name_opt = endpoint_config->get<std::string>("database_name");
        if (!database_name_opt) {
            throw std::runtime_error("cannot find database_name at the section in the configuration");
        }
        database_name_ = database_name_opt.value();
        if (database_name_.empty()) {
            throw std::runtime_error("database_name in ipc_endpoint section should not be empty");
        }

        auto threads_opt = endpoint_config->get<std::size_t>("threads");
        if (!threads_opt) {
            throw std::runtime_error("cannot find thread_pool_size at the section in the configuration");
        }
        auto threads = threads_opt.value();

        auto datachannel_buffer_size_opt = endpoint_config->get<std::size_t>("datachannel_buffer_size");
        if (!datachannel_buffer_size_opt) {
            throw std::runtime_error("cannot find datachannel_buffer_size at the section in the configuration");
        }
        datachannel_buffer_size_ = datachannel_buffer_size_opt.value() * 1024;  // in KB
        VLOG_LP(log_debug) << "datachannel_buffer_size = " << datachannel_buffer_size_ << " bytes";

        auto max_datachannel_buffers_opt = endpoint_config->get<std::size_t>("max_datachannel_buffers");
        if (!max_datachannel_buffers_opt) {
            throw std::runtime_error("cannot find max_datachannel_buffers at the section in the configuration");
        }
        max_datachannel_buffers_ = max_datachannel_buffers_opt.value();
        VLOG_LP(log_debug) << "max_datachannel_buffers = " << max_datachannel_buffers_;

        // connection channel
        container_ = std::make_unique<tateyama::common::wire::connection_container>(database_name_, threads);

        // worker objects
        workers_.resize(threads);

        // set maximum thread size to status objects
        status_->set_maximum_sessions(threads);

        // output configuration to be used
        LOG(INFO) << tateyama::endpoint::common::ipc_endpoint_config_prefix
                  << "database_name: " << database_name_opt.value() << ", "
                  << "database name.";
        LOG(INFO) << tateyama::endpoint::common::ipc_endpoint_config_prefix
                  << "threads: " << threads_opt.value() << ", "
                  << "the number of maximum sesstions.";
        LOG(INFO) << tateyama::endpoint::common::ipc_endpoint_config_prefix
                  << "datachannel_buffer_size: " << datachannel_buffer_size_opt.value() << ", "
                  << "datachannel_buffer_size in KB.";
        LOG(INFO) << tateyama::endpoint::common::ipc_endpoint_config_prefix
                  << "max_datachannel_buffers: " << max_datachannel_buffers_opt.value() << ", "
                  << "the number of maximum datachannel buffers.";
    }

    void operator()() {
        auto& connection_queue = container_->get_connection_queue();
        proc_mutex_file_ = status_->mutex_file();
        status_->set_database_name(database_name_);
        arrive_and_wait();

        while(true) {
            try {
                auto session_id = connection_queue.listen();
                if (connection_queue.is_terminated()) {
                    VLOG_LP(log_trace) << "receive terminate request";
                    for (auto& worker : workers_) {
                        if (worker) {
                            worker->terminate();
                            if (auto rv = worker->future_.wait_for(std::chrono::seconds(0)); rv != std::future_status::ready) {
                                VLOG_LP(log_trace) << "exit: remaining thread " << worker->session_id_;
                            }
                        }
                    }
                    workers_.clear();
                    connection_queue.confirm_terminated();
                    break;
                }
                std::string session_name = database_name_;
                session_name += "-";
                session_name += std::to_string(session_id);
                auto wire = std::make_shared<tateyama::common::wire::server_wire_container_impl>(session_name, proc_mutex_file_, datachannel_buffer_size_, max_datachannel_buffers_);
                std::size_t index = connection_queue.accept(session_id);
                VLOG_LP(log_trace) << "create session wire: " << session_name << " at index " << index;
                status_->add_shm_entry(session_id, index);
                auto& worker = workers_.at(index);
                worker = std::make_shared<server::Worker>(*router_, session_id, std::move(wire),
                                                              [&connection_queue, index](){ connection_queue.disconnect(index); });
                worker->task_ = std::packaged_task<void()>([&]{worker->run();});
                worker->future_ = worker->task_.get_future();
                worker->thread_ = std::thread(std::move(worker->task_));
            } catch (std::exception& ex) {
                LOG_LP(ERROR) << ex.what();
                workers_.clear();
                break;    // shutdown the ipc_listener
            }
        }
    }

    void terminate() {
        container_->get_connection_queue().request_terminate();
    }

    void arrive_and_wait() {
        sync.wait();
    }

    void print_diagnostic(std::ostream& os) {
        os << "/:tateyama:ipc_endpoint print diagnostics start" << std::endl;
        bool cont{};
        for (auto && e : workers_) {
            if (std::shared_ptr<Worker> worker = e; worker) {
                if (!worker->terminated_) {
                    os << (cont ? "live sessions " : ", ") << worker->session_id_ << std::endl;
                }
            }
        }
        os << "session_id accepted = " << container_->session_id_accepted() <<", pending requests = " << container_->pending_requests() << std::endl;
        os << "/:tateyama:ipc_endpoint print diagnostics end" << std::endl;
    }

private:
    const std::shared_ptr<api::configuration::whole> cfg_{};
    const std::shared_ptr<framework::routing_service> router_{};
    const std::shared_ptr<status_info::resource::bridge> status_{};
    std::unique_ptr<tateyama::common::wire::connection_container> container_{};
    std::vector<std::shared_ptr<Worker>> workers_{};
    std::string database_name_;
    std::string proc_mutex_file_;
    std::size_t datachannel_buffer_size_{};
    std::size_t max_datachannel_buffers_{};

    boost::barrier sync{2};
};

}
