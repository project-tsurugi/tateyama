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
#include "tateyama/authentication/resource/bridge.h"

#include "tateyama/endpoint/common/listener_common.h"
#include "tateyama/endpoint/common/logging.h"
#include "tateyama/endpoint/common/pointer_comp.h"
#include "tateyama/endpoint/stream/stream_response.h"
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
class stream_listener : public tateyama::endpoint::common::listener_common {
    static constexpr std::size_t connection_socket_timeout = 1000;

public:
    explicit stream_listener(tateyama::framework::environment& env)
        : cfg_(env.configuration()),
          router_(env.service_repository().find<framework::routing_service>()),
          status_(env.resource_repository().find<status_info::resource::bridge>()),
          session_(env.resource_repository().find<session::resource::bridge>()),
          conf_(tateyama::endpoint::common::connection_type::stream, session_,
                status_->database_info(),
                authentication_bridge(env)),
          stream_metrics_(env) {

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

        auto allow_blob_privileged_opt = endpoint_config->get<bool>("allow_blob_privileged");
        if (!allow_blob_privileged_opt) {
            throw std::runtime_error("cannot find allow_blob_privileged at the ipc_endpoint section in the configuration");
        }
        auto allow_blob_privileged = allow_blob_privileged_opt.value();
        VLOG_LP(log_debug) << "allow_blob_privileged = " << std::boolalpha << allow_blob_privileged << std::noboolalpha;
        conf_.allow_blob_privileged(allow_blob_privileged);

        // connection stream
        connection_socket_ = std::make_unique<connection_socket>(port, connection_socket_timeout);

        // worker objects
        workers_.resize(threads);

        // output configuration to be used
        LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                  << "port: " << port << ", "
                  << "port number to listen for TCP/IP connections.";
        LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                  << "threads: " << threads << ", "
                  << "the number of maximum sessions.";

        // session timeout
        if (auto* session_config = cfg_->get_section("session"); session_config) {
            auto enable_timeout_opt = session_config->get<bool>("enable_timeout");
            if (!enable_timeout_opt) {
                throw std::runtime_error("cannot find enable_timeout at the session section in the configuration");
            }

            auto enable_timeout = enable_timeout_opt.value();
            LOG(INFO) << tateyama::endpoint::common::session_config_prefix
                      << "enable_timeout: " << enable_timeout << ", "
                      << "whether timeout is enabled or not.";

            if (enable_timeout) {
                auto refresh_timeout_opt = session_config->get<std::size_t>("refresh_timeout");
                if (!refresh_timeout_opt) {
                    throw std::runtime_error("cannot find thread_pool_size at the session section in the configuration");
                }
                auto refresh_timeout = refresh_timeout_opt.value();
                auto max_refresh_timeout_opt = session_config->get<std::size_t>("max_refresh_timeout");
                if (!max_refresh_timeout_opt) {
                    throw std::runtime_error("cannot find thread_pool_size at the session section in the configuration");
                }
                auto  max_refresh_timeout = max_refresh_timeout_opt.value();
                conf_.set_timeout(refresh_timeout, max_refresh_timeout);
                
                LOG(INFO) << tateyama::endpoint::common::session_config_prefix
                          << "refresh_timeout: " << refresh_timeout << ", "
                          << "refresh timeout in seconds.";
                LOG(INFO) << tateyama::endpoint::common::session_config_prefix
                          << "max_refresh_timeout: " << max_refresh_timeout << ", "
                          << "maximum refresh timeout in seconds.";
            }
        }

        LOG(INFO) << tateyama::endpoint::common::ipc_endpoint_config_prefix
                  << "allow_blob_privileged: " << std::boolalpha << allow_blob_privileged << std::noboolalpha << ", "
                  << "whether permission to handle blobs in privileged mode is granted or not.";
    }

    ~stream_listener() override {
        connection_socket_->close();
    }

    stream_listener(stream_listener const& other) = delete;
    stream_listener& operator=(stream_listener const& other) = delete;
    stream_listener(stream_listener&& other) noexcept = delete;
    stream_listener& operator=(stream_listener&& other) noexcept = delete;

    void operator()() override {
        pthread_setname_np(pthread_self(), "tcp_listener");
        session_id_ = 0x1000000000000000LL;  // initial value

        arrive_and_wait();
        while(true) {

            std::unique_ptr<stream_socket> stream{};
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

            DVLOG_LP(log_trace) << "created session stream: " << session_id_;
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
                    auto worker_decline = std::make_shared<stream_worker>(*router_, conf_, session_id_, std::move(stream), true);
                    auto* worker = worker_decline.get();
                    {
                        std::unique_lock<std::mutex> lock(mtx_undertakers_);
                        undertakers_.emplace(std::move(worker_decline));
                    }
                    worker->invoke([worker]{
                        try {
                            worker->run();
                        } catch(std::exception &ex) {
                            LOG(ERROR) << "ipc_endpoint worker thread got an exception: " << ex.what();
                        }
                    });
                    LOG_LP(INFO) << "the number of sessions exceeded the limit (" << workers_.size() << ")";
                } catch (std::runtime_error &ex) {
                    LOG_LP(ERROR) << ex.what();
                }
            } else {
                try {
                    auto& worker_entry = workers_.at(index);
                    {
                        std::unique_lock<std::mutex> lock(mtx_workers_);
                        worker_entry = std::make_shared<stream_worker>(*router_, conf_, session_id_, std::move(stream), false);
                    }
                    stream_metrics_.increase();
                    worker_entry->invoke([this, index]{
                        auto& worker = workers_.at(index);
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
                            undertakers_.emplace(std::move(worker));
                        }
                        stream_metrics_.decrease();
                    });
                    session_id_++;
                } catch (std::exception& ex) {
                    LOG_LP(ERROR) << ex.what();
                }
            }
        }
        confirm_workers_termination();
    }

    void arrive_and_wait() override {
        sync.wait();
    }

    void terminate() override {
        connection_socket_->request_terminate();
    }

    void print_diagnostic(std::ostream& os) override {
        os << "/:tateyama:stream_endpoint print diagnostics start\n";
        {
            std::unique_lock<std::mutex> lock(mtx_workers_);
            os << "  live sessions\n";
            for (auto && worker : workers_) {
                if (worker) {
                    worker->print_diagnostic(os, !worker->is_terminated());
                }
            }
        }
        {
            std::unique_lock<std::mutex> lock(mtx_undertakers_);
            for (auto && worker : undertakers_) {
                if (worker) {
                    worker->print_diagnostic(os, false);
                }
            }
        }
        os << "  connection status\n"
              "    session_id accepted = " << session_id_ << "\n"
              "/:tateyama:stream_endpoint print diagnostics end\n";
    }

    void foreach_request(const callback& func) override {
        std::unique_lock<std::mutex> lock(mtx_workers_);
        for (auto && e : workers_) {
            if (const std::shared_ptr<stream_worker>& worker = e; worker) {
                if (!worker->is_terminated()) {
                    worker->foreach_request(func);
                }
            }
        }
    }

private:
    const std::shared_ptr<api::configuration::whole> cfg_;
    const std::shared_ptr<framework::routing_service> router_;
    const std::shared_ptr<status_info::resource::bridge> status_;
    const std::shared_ptr<tateyama::session::resource::bridge> session_;
    tateyama::endpoint::common::configuration conf_;
    tateyama::endpoint::stream::metrics::stream_metrics stream_metrics_;

    std::size_t session_id_{};
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
