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
#include <string>
#include <exception>
#include <functional>
#include <mutex>
#include <chrono>
#include <vector>
#include <set>
#include <csignal>

#include <boost/thread/barrier.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/api/configuration.h>
#include <tateyama/session/resource/bridge.h>

#include "tateyama/endpoint/stream/stream_response.h"
#include "tateyama/endpoint/common/logging.h"
#include "tateyama/endpoint/common/pointer_comp.h"
#include "tateyama/endpoint/stream/metrics/stream_metrics.h"
#include "stream_worker.h"

namespace tateyama::endpoint::stream::bootstrap {

// should be in sync one in bootstrap
struct stream_endpoint_context {
    std::unordered_map<std::string, std::string> options_{};
};

/**
 * @brief stream endpoint provider
 * @details
 */
class stream_listener {
public:
    explicit stream_listener(tateyama::framework::environment& env)
        : cfg_(env.configuration()),
          router_(env.service_repository().find<framework::routing_service>()),
          status_(env.resource_repository().find<status_info::resource::bridge>()),
          session_(env.resource_repository().find<session::resource::bridge>()),
          stream_metrics_(env)
        {

        auto endpoint_config = cfg_->get_section("stream_endpoint");
        if (endpoint_config == nullptr) {
            LOG_LP(ERROR) << "cannot find stream_endpoint section in the configuration";
            exit(1);
        }
        auto port_opt = endpoint_config->get<int>("port");
        if (!port_opt) {
            LOG_LP(ERROR) << "cannot port at the section in the configuration";
            exit(1);
        }
        auto port = port_opt.value();
        auto threads_opt = endpoint_config->get<std::size_t>("threads");
        if (!threads_opt) {
            LOG_LP(ERROR) << "cannot find thread_pool_size at the section in the configuration";
            exit(1);
        }
        auto threads = threads_opt.value();

        // dev_idle_work_interval need not be provided in the default configuration.
        std::size_t timeout = 1000;
        auto timeout_opt = endpoint_config->get<std::size_t>("dev_idle_work_interval");
        if (timeout_opt) {
            timeout = timeout_opt.value();
        }

        // connection stream
        connection_socket_ = std::make_unique<connection_socket>(port, timeout);

        // worker objects
        workers_.resize(threads);

        // output configuration to be used
        LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                  << "port: " << port_opt.value() << ", "
                  << "port number to listen for TCP/IP connections.";
        LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                  << "threads: " << threads_opt.value() << ", "
                  << "the number of maximum sessions.";
        if (timeout != 1000) {
            LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                      << "dev_idle_work_interval has changed to: " << timeout_opt.value() << ", "
                      << "the idle work interval for stream listener in millisecond.";
        }
    }
    ~stream_listener() {
        connection_socket_->close();
    }

    stream_listener(stream_listener const& other) = delete;
    stream_listener& operator=(stream_listener const& other) = delete;
    stream_listener(stream_listener&& other) noexcept = delete;
    stream_listener& operator=(stream_listener&& other) noexcept = delete;

    void operator()() {
        std::size_t session_id = 0x1000000000000000LL;

        arrive_and_wait();
        while(true) {

            std::shared_ptr<stream_socket> stream{};
            try {
                stream = connection_socket_->accept([this](){care_undertakers();});
                if (!stream) {  // received termination request.
                    terminate_workers();
                    break;
                }
            } catch (std::exception& ex) {
                LOG_LP(ERROR) << ex.what();
                continue;
            }

            DVLOG_LP(log_trace) << "created session stream: " << session_id;
            std::size_t index = 0;
            bool found = false;
            for (; index < workers_.size() ; index++) {
                auto& worker = workers_.at(index);
                if (!worker) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                try {
                    auto worker_decline = std::make_shared<stream_worker>(*router_, session_id, std::move(stream), status_->database_info(), true);
                    auto* worker = worker_decline.get();
                    {
                        std::unique_lock<std::mutex> lock(mtx_undertakers_);
                        undertakers_.emplace(std::move(worker_decline));
                    }
                    worker->invoke([worker]{worker->run();});
                    LOG_LP(INFO) << "the number of sessions exceeded the limit (" << workers_.size() << ")";
                } catch (std::runtime_error &ex) {
                    LOG_LP(ERROR) << ex.what();
                }
            } else {
                try {
                    auto& worker_entry = workers_.at(index);
                    {
                        std::unique_lock<std::mutex> lock(mtx_workers_);
                        worker_entry = std::make_shared<stream_worker>(*router_, session_id, std::move(stream), status_->database_info(), false, session_);
                    }
                    stream_metrics_.increase();
                    worker_entry->invoke([this, index]{
                        auto& worker = workers_.at(index);
                        worker->register_worker_in_context(worker);
                        worker->run();
                        {
                            std::unique_lock<std::mutex> lock_w(mtx_workers_);
                            std::unique_lock<std::mutex> lock_u(mtx_undertakers_);
                            undertakers_.emplace(std::move(worker));
                        }
                        stream_metrics_.decrease();
                    });
                    session_id++;
                } catch (std::exception& ex) {
                    LOG_LP(ERROR) << ex.what();
                }
            }
        }
        confirm_workers_termination();
    }

    void arrive_and_wait() {
        sync.wait();
    }

    void terminate() {
        connection_socket_->request_terminate();
    }

private:
    const std::shared_ptr<api::configuration::whole> cfg_;
    const std::shared_ptr<framework::routing_service> router_;
    const std::shared_ptr<status_info::resource::bridge> status_;
    const std::shared_ptr<tateyama::session::resource::bridge> session_;
    tateyama::endpoint::stream::metrics::stream_metrics stream_metrics_;

    std::unique_ptr<connection_socket> connection_socket_{};
    std::vector<std::shared_ptr<stream_worker>> workers_{};  // accessed only by the listner thread, thus mutual exclusion in unnecessary.
    std::set<std::shared_ptr<stream_worker>, tateyama::endpoint::common::pointer_comp<stream_worker>> undertakers_{};
    std::mutex mtx_workers_{};
    std::mutex mtx_undertakers_{};

    boost::barrier sync{2};

    bool care_undertakers() {
        std::unique_lock<std::mutex> lock(mtx_undertakers_);
        for (auto it{undertakers_.begin()}, end{undertakers_.end()}; it != end; ) {
            if ((*it)->is_quiet()) {
                it = undertakers_.erase(it);
            }
            else {
                ++it;
            }
        }
        return undertakers_.empty();
    }
    void terminate_workers() {
        std::unique_lock<std::mutex> lock(mtx_workers_);
        for (auto&& worker : workers_) {
            if (worker) {
                worker->terminate();
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
