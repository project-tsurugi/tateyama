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
#pragma once

#include <memory>
#include <string>
#include <exception>
#include <chrono>
#include <csignal>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/api/configuration.h>

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
            LOG_LP(ERROR) << "cannot find ipc_endpoint section in the configuration";
            exit(1);
        }
        auto database_name_opt = endpoint_config->get<std::string>("database_name");
        if (!database_name_opt) {
            LOG_LP(ERROR) << "cannot find database_name at the section in the configuration";
            exit(1);
        }
        database_name_ = database_name_opt.value();
        auto threads_opt = endpoint_config->get<std::size_t>("threads");
        if (!threads_opt) {
            LOG_LP(ERROR) << "cannot find thread_pool_size at the section in the configuration";
            exit(1);
        }
        auto threads = threads_opt.value();

        // connection channel
        container_ = std::make_unique<tateyama::common::wire::connection_container>(database_name_, threads);

        // worker objects
        workers_.resize(threads);

        // set maximum thread size to status objects
        status_->set_maximum_sessions(threads);
    }

    void operator()() {
        auto& connection_queue = container_->get_connection_queue();
        proc_mutex_file_ = status_->mutex_file();
        status_->set_database_name(database_name_);

        while(true) {
            auto session_id = connection_queue.listen();
            if (connection_queue.is_terminated()) {
                VLOG_LP(log_trace) << "receive terminate request";
                for (auto& worker : workers_) {
                    if (worker) {
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
            auto wire = std::make_unique<tateyama::common::wire::server_wire_container_impl>(session_name, proc_mutex_file_);
            std::size_t index = connection_queue.accept(session_id);
            VLOG_LP(log_trace) << "create session wire: " << session_name << " at index " << index;
            try {
                status_->add_shm_entry(session_id, index);
                auto& worker = workers_.at(index);
                worker = std::make_unique<server::Worker>(*router_, session_id, std::move(wire),
                                                          [&connection_queue, index, session_name](){
                                                              connection_queue.disconnect(index);
                                                              VLOG_LP(log_trace) << "destroy session wire: " << session_name << " at index " << index;
                                                          });
                worker->task_ = std::packaged_task<void()>([&]{worker->run();});
                worker->future_ = worker->task_.get_future();
                worker->thread_ = std::thread(std::move(worker->task_));
            } catch (std::exception& ex) {
                LOG_LP(ERROR) << ex.what();
                workers_.clear();
                break;
            }
        }
    }

    void terminate() {
        container_->get_connection_queue().request_terminate();
    }

private:
    const std::shared_ptr<api::configuration::whole> cfg_{};
    const std::shared_ptr<framework::routing_service> router_{};
    const std::shared_ptr<status_info::resource::bridge> status_{};
    std::unique_ptr<tateyama::common::wire::connection_container> container_{};
    std::vector<std::unique_ptr<Worker>> workers_{};
    std::string database_name_;
    std::string proc_mutex_file_;
};

}
