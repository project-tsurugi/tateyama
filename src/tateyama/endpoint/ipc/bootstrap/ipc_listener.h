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
#include <vector>
#include <set>
#include <string>
#include <exception>
#include <thread>
#include <chrono>
#include <mutex>
#include <csignal>

#include <boost/thread/barrier.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/api/configuration.h>
#include <tateyama/session/resource/bridge.h>

#include "tateyama/endpoint/common/logging.h"
#include "tateyama/endpoint/common/pointer_comp.h"
#include "worker.h"

namespace tateyama::endpoint::ipc::bootstrap {

/**
 * @brief ipc listener
 */
class ipc_listener {
public:
    explicit ipc_listener(tateyama::framework::environment& env)
        : cfg_(env.configuration()),
          router_(env.service_repository().find<framework::routing_service>()),
          status_(env.resource_repository().find<status_info::resource::bridge>()),
          session_(env.resource_repository().find<session::resource::bridge>()) {

        auto endpoint_config = cfg_->get_section("ipc_endpoint");
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
        container_ = std::make_unique<connection_container>(database_name_, threads);

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
                  << "the number of maximum sessions.";
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
        arrive_and_wait();

        while(true) {
            try {
                care_undertakers();
                auto session_id = connection_queue.listen();
                if (session_id == 0) {  // means timeout
                    continue;
                }
                if (connection_queue.is_terminated()) {
                    VLOG_LP(log_trace) << "receive terminate request";
                    terminate_workers();
                    connection_queue.confirm_terminated();
                    break;    // shutdown the ipc_listener
                }
                std::string session_name = database_name_;
                session_name += "-";
                session_name += std::to_string(session_id);
                auto wire = std::make_shared<server_wire_container_impl>(session_name, proc_mutex_file_, datachannel_buffer_size_, max_datachannel_buffers_);
                std::size_t index = connection_queue.accept(session_id);
                VLOG_LP(log_trace) << "create session wire: " << session_name << " at index " << index;
                status_->add_shm_entry(session_id, index);
                auto& worker_entry = workers_.at(index);
                worker_entry = std::make_shared<Worker>(*router_, session_id, std::move(wire), status_->database_info(), session_);
                worker_entry->invoke([this, index, &connection_queue]{
                    std::shared_ptr<Worker> worker = workers_.at(index);
                    worker->register_worker_in_context(worker);
                    worker->run();
                    if (!worker->is_quiet()) {
                        std::unique_lock<std::mutex> lock(mtx_undertakers_);
                        undertakers_.emplace(std::move(worker));
                    }
                    connection_queue.disconnect(index);
                });
            } catch (std::exception& ex) {
                LOG_LP(ERROR) << ex.what();
                terminate_workers();
                break;
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
                if (!worker->terminated()) {
                    os << (cont ? "live sessions " : ", ") << worker->session_id() << std::endl;
                }
            }
        }
        os << "session_id accepted = " << container_->session_id_accepted() <<", pending requests = " << container_->pending_requests() << std::endl;
        os << "/:tateyama:ipc_endpoint print diagnostics end" << std::endl;
    }

private:
    const std::shared_ptr<api::configuration::whole> cfg_;
    const std::shared_ptr<framework::routing_service> router_;
    const std::shared_ptr<status_info::resource::bridge> status_;
    const std::shared_ptr<session::resource::bridge> session_;

    std::unique_ptr<connection_container> container_{};
    std::vector<std::shared_ptr<Worker>> workers_{};
    std::set<std::shared_ptr<Worker>, tateyama::endpoint::common::pointer_comp<Worker>> undertakers_{};
    std::string database_name_;
    std::string proc_mutex_file_;
    std::size_t datachannel_buffer_size_{};
    std::size_t max_datachannel_buffers_{};
    std::mutex mtx_undertakers_{};

    boost::barrier sync{2};

    bool care_undertakers() {
        std::unique_lock<std::mutex> lock(mtx_undertakers_);
        for (auto it{undertakers_.begin()}, end{undertakers_.end()}; it != end; ) {
            if ((*it)->is_quiet()) {
                it = undertakers_.erase(it);
            } else {
                ++it;
            }
        }
        return undertakers_.empty();
    }
    void terminate_workers() {
        for (auto& worker : workers_) {
            if (worker) {
                worker->terminate();
                while (true) {
                    bool message_output{false};
                    if (auto rv = worker->wait_for(); rv != std::future_status::ready) {
                        if (!message_output) {
                            VLOG_LP(log_trace) << "wait for remaining worker thread, session id = " << worker->session_id();
                            message_output = true;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        continue;
                    }
                    break;
                }
            }
        }
        workers_.clear();
    }
};

}
