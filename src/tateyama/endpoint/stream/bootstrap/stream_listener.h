/*
 * Copyright 2019-2022 tsurugi project.
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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/api/endpoint/service.h>
#include <tateyama/api/endpoint/provider.h>
#include <tateyama/api/registry.h>
#include <tateyama/framework/endpoint_broker.h>
#include <tateyama/api/configuration.h>

#include <tateyama/endpoint/stream/stream_response.h>
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
    explicit stream_listener(std::shared_ptr<api::configuration::whole> cfg, std::shared_ptr<framework::endpoint_broker> broker) :
        cfg_(cfg),
        broker_(std::move(broker))
    {
        auto endpoint_config = cfg->get_section("stream_endpoint");
        if (endpoint_config == nullptr) {
            LOG(ERROR) << "cannot find stream_endpoint section in the configuration";
            exit(1);
        }
        int port{};
        if (!endpoint_config->get<>("port", port)) {
            LOG(ERROR) << "cannot port at the section in the configuration";
            exit(1);
        }
        std::size_t threads{};
        if (!endpoint_config->get<>("threads", threads)) {
            LOG(ERROR) << "cannot find thread_pool_size at the section in the configuration";
            exit(1);
        }
        // connection stream
        connection_socket_ = std::make_unique<tateyama::common::stream::connection_socket>(port);

        // worker objects
        workers_.reserve(threads);
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

        while(true) {
            if (auto stream = connection_socket_->accept(); stream != nullptr) {
                std::string session_name = std::to_string(session_id);
                stream->send(session_name);
                VLOG(log_debug) << "created session stream: " << session_name;
                std::size_t index = 0;
                for (; index < workers_.size() ; index++) {
                    if (auto rv = workers_.at(index)->future_.wait_for(std::chrono::seconds(0)) ; rv == std::future_status::ready) {
                        break;
                    }
                }
                if (workers_.size() < (index + 1)) {
                    workers_.resize(index + 1);
                }
                try {
                    std::unique_ptr<stream_worker> &worker = workers_.at(index);
                    worker = std::make_unique<stream_worker>(*broker_, session_id, std::move(stream));
                    worker->task_ = std::packaged_task<void()>([&]{worker->run();});
                    worker->future_ = worker->task_.get_future();
                    worker->thread_ = std::thread(std::move(worker->task_));
                    session_id++;
                } catch (std::exception &ex) {
                    LOG(ERROR) << ex.what();
                    workers_.clear();
                    break;
                }
            } else {
                break;
            }
        }
    }

    void terminate() {
        connection_socket_->request_terminate();
    }

private:
    std::shared_ptr<api::configuration::whole> cfg_{};
    std::shared_ptr<framework::endpoint_broker> broker_{};
    std::unique_ptr<tateyama::common::stream::connection_socket> connection_socket_{};
    std::vector<std::unique_ptr<stream_worker>> workers_{};
};

}  // tateyama::server