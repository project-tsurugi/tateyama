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
#include <iostream>
#include <chrono>
#include <csignal>

#include <boost/thread/barrier.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/api/configuration.h>

#include "tateyama/endpoint/stream/stream_response.h"
#include "tateyama/endpoint/common/logging.h"
#include "stream_worker.h"

namespace tateyama::server {

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

    // in case for session limit
    class undertaker {
    public:
        explicit undertaker(std::shared_ptr<tateyama::common::stream::stream_socket> stream) : stream_(std::move(stream)) {}
        void operator()() {
            while (stream_->wait_hello(""));
            done = true;
        }
        [[nodiscard]] bool is_done() const {
            return done;
        }
    private:
        std::shared_ptr<tateyama::common::stream::stream_socket> stream_;
        bool done{};
    };

    explicit stream_listener(tateyama::framework::environment& env)
        : cfg_(env.configuration()),
          router_(env.service_repository().find<framework::routing_service>()),
          status_(env.resource_repository().find<status_info::resource::bridge>()) {

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

        // connection stream
        connection_socket_ = std::make_unique<tateyama::common::stream::connection_socket>(port);

        // worker objects
        workers_.resize(threads);

        // output configuration to be used
        LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                  << "port: " << port_opt.value() << ", "
                  << "port number to listen for TCP/IP connections.";
        LOG(INFO) << tateyama::endpoint::common::stream_endpoint_config_prefix
                  << "threads: " << threads_opt.value() << ", "
                  << "the number of maximum sesstions.";
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
            undertakers_.erase(std::remove_if(std::begin(undertakers_), std::end(undertakers_), [](std::unique_ptr<undertaker>& ut){ return ut->is_done(); }), std::cend(undertakers_));
            std::shared_ptr<tateyama::common::stream::stream_socket> stream{};
            try {
                stream = connection_socket_->accept();
            } catch (std::exception& ex) {
                LOG_LP(ERROR) << ex.what();
                continue;
            }
            if (stream != nullptr) {
                DVLOG_LP(log_trace) << "created session stream: " << session_id;
                std::size_t index = 0;
                bool found = false;
                for (; index < workers_.size() ; index++) {
                    auto& worker = workers_.at(index);
                    if (!worker) {
                        found = true;
                        break;
                    }
                    if (auto rv = worker->wait_for(); rv == std::future_status::ready) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    stream->decline();
                    auto ut = std::make_unique<undertaker>(std::move(stream));
                    auto t = std::thread(std::ref(*ut));
                    t.detach();
                    undertakers_.emplace_back(std::move(ut));
                    LOG_LP(ERROR) << "the number of sessions exceeded the limit (" << workers_.size() << ")";
                    continue;
                }
                auto& worker = workers_.at(index);
                try {
                    worker = std::make_unique<stream_worker>(*router_, session_id, std::move(stream), status_->database_info());
                } catch (std::exception& ex) {
                    LOG_LP(ERROR) << ex.what();
                    continue;
                }
                worker->invoke([&]{worker->run();});
                session_id++;
            } else {  // connect via pipe (request_terminate)
                break;
            }
        }
    }

    void arrive_and_wait() {
        sync.wait();
    }

    void terminate() {
        connection_socket_->request_terminate();
    }

private:
    const std::shared_ptr<api::configuration::whole> cfg_{};
    const std::shared_ptr<framework::routing_service> router_{};
    const std::shared_ptr<status_info::resource::bridge> status_{};
    std::unique_ptr<tateyama::common::stream::connection_socket> connection_socket_{};
    std::vector<std::unique_ptr<stream_worker>> workers_{};
    std::vector<std::unique_ptr<undertaker>> undertakers_{};

    boost::barrier sync{2};
};

}  // tateyama::server
