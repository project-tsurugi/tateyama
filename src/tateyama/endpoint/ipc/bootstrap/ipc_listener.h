/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include "tateyama/endpoint/ipc/metrics/ipc_metrics.h"
#include "ipc_worker.h"

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
          session_(env.resource_repository().find<session::resource::bridge>()),
          ipc_metrics_(env)
        {

        auto* endpoint_config = cfg_->get_section("ipc_endpoint");
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

        auto admin_sessions_opt = endpoint_config->get<std::size_t>("admin_sessions");
        if (!admin_sessions_opt) {
            throw std::runtime_error("cannot find admin_sessions at the section in the configuration");
        }
        if (admin_sessions_opt.value() > UINT8_MAX) {
            throw std::runtime_error("admin_sessions should be less than or equal to UINT8_MAX");
        }
        auto admin_sessions = admin_sessions_opt.value();
        VLOG_LP(log_debug) << "admin_sessions = " << admin_sessions;

        // connection channel
        container_ = std::make_unique<connection_container>(database_name_, threads, admin_sessions);

        // worker objects
        workers_.resize(threads + admin_sessions);

        // set maximum thread size to status objects
        status_->set_maximum_sessions(threads + admin_sessions);

        // set memory usage parameters to ipc_metrics
        ipc_metrics_.set_memory_parameters(connection_container::fixed_memory_size(threads + admin_sessions),
                                           server_wire_container_impl::proportional_memory_size(datachannel_buffer_size_, max_datachannel_buffers_));

        // set maximum writer count from sql.default_partitions
        auto* sql_config = cfg_->get_section("sql");
        if (sql_config == nullptr) {
            throw std::runtime_error("cannot find sql section in the configuration");
        }
        auto default_partitions_opt = sql_config->get<std::size_t>("default_partitions");
        if (default_partitions_opt) {
            if (writer_count_ < default_partitions_opt.value()) {
                writer_count_ = default_partitions_opt.value();
            }
        }

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
        LOG(INFO) << tateyama::endpoint::common::ipc_endpoint_config_prefix
                  << "admin_sessions: " << admin_sessions << ", "
                  << "the number of maximum admin sessions.";
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
                auto slot_id = connection_queue.slot();
                auto slot_index = tateyama::common::wire::connection_queue::reset_admin(slot_id);
                try {
                    std::string session_name = database_name_;
                    session_name += "-";
                    session_name += std::to_string(session_id);
                    auto wire = std::make_shared<server_wire_container_impl>(session_name, proc_mutex_file_, datachannel_buffer_size_, max_datachannel_buffers_, [this, session_id, slot_index](){status_->remove_shm_entry(session_id, slot_index);});
                    VLOG_LP(log_trace) << "create session wire: " << session_name << " at index " << slot_index;
                    status_->add_shm_entry(session_id, slot_index);

                    auto& worker_entry = workers_.at(slot_index);
                    std::unique_lock<std::mutex> lock(mtx_workers_);
                    worker_entry = std::make_shared<ipc_worker>(*router_, session_id, std::move(wire), status_->database_info(), writer_count_, session_);
                    connection_queue.accept(slot_id, session_id);
                    ipc_metrics_.increase();
                    worker_entry->invoke([this, slot_id, slot_index, &connection_queue]{
                        auto& worker = workers_.at(slot_index);
                        worker->register_worker_in_context(worker);
                        try {
                            worker->run();
                        } catch(std::exception &ex) {
                            LOG(ERROR) << "ipc_endpoint worker thread got an exception: " << ex.what();
                        }
                        worker->dispose_session_store();
                        {
                            std::unique_lock<std::mutex> lock_w(mtx_workers_);
                            std::unique_lock<std::mutex> lock_u(mtx_undertakers_);
                            auto* wp = worker.get();
                            undertakers_.emplace(std::move(worker));
                            connection_queue.disconnect(slot_id);
                            wp->delete_hook();
                        }
                        ipc_metrics_.decrease();
                    });
                } catch (std::exception& ex) {
                    LOG_LP(ERROR) << ex.what();
                    connection_queue.reject(slot_id);
                    continue;
                }
            } catch (std::exception& ex) {
                LOG_LP(ERROR) << ex.what();
                continue;
            }
        }
        confirm_workers_termination();
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

        std::unique_lock<std::mutex> lock(mtx_workers_);
        for (auto && e : workers_) {
            if (std::shared_ptr<ipc_worker> worker = e; worker) {
                if (!worker->is_terminated()) {
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
    tateyama::endpoint::ipc::metrics::ipc_metrics ipc_metrics_;

    std::unique_ptr<connection_container> container_{};
    std::vector<std::shared_ptr<ipc_worker>> workers_{};
    std::set<std::shared_ptr<ipc_worker>, tateyama::endpoint::common::pointer_comp<ipc_worker>> undertakers_{};
    std::string database_name_;
    std::string proc_mutex_file_;
    std::size_t datachannel_buffer_size_{};
    std::size_t max_datachannel_buffers_{};
    std::size_t writer_count_{ipc_response::default_writer_count};
    std::mutex mtx_workers_{};
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
        tateyama::status_info::shutdown_type shutdown_type = status_->get_shutdown_request();
        {
            std::unique_lock<std::mutex> lock(mtx_workers_);
            for (auto&& worker : workers_) {
                if (worker) {
                    worker->terminate(shutdown_type == tateyama::status_info::shutdown_type::graceful ?
                                      tateyama::session::shutdown_request_type::graceful :
                                      tateyama::session::shutdown_request_type::forceful);
                }
            }
        }
    }
    void confirm_workers_termination() {
        bool message_output{false};
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mtx_workers_);
                bool worker_remain{false};
                for (auto& worker : workers_) {
                    if (worker) {
                        if (!message_output) {  // message output for the first worker only
                            VLOG_LP(log_trace) << "wait for remaining worker thread, session id = " << worker->session_id();
                            message_output = true;
                        }
                        worker_remain = true;
                    }
                }
                if (!worker_remain) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        while (!care_undertakers()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
};

}
