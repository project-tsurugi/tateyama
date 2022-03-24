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
#include <tateyama/api/environment.h>

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
class stream_provider : public tateyama::api::endpoint::provider {
private:
    class listener {
    public:
        listener(api::environment& env, std::size_t size, std::uint32_t port) : env_(env) {
            // connection stream
            connection_socket_ = std::make_unique<tateyama::common::stream::connection_socket>(port);

            // worker objects
            workers_.reserve(size);
        }
        ~listener() {
            connection_socket_->close();
        }

        listener(listener const& other) = delete;
        listener& operator=(listener const& other) = delete;
        listener(listener&& other) noexcept = delete;
        listener& operator=(listener&& other) noexcept = delete;
        
        void operator()() {
            std::size_t session_id = 0;

            while(true) {
                if (auto stream = connection_socket_->accept(); stream != nullptr) {
                    std::string session_name = std::to_string(session_id);
                    stream->send(tateyama::common::stream::stream_socket::RESPONSE_SESSION_HELLO_OK, 0, session_name);
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
                        worker = std::make_unique<stream_worker>(*env_.endpoint_service(), session_id, std::move(stream));
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
        api::environment& env_;
        std::unique_ptr<tateyama::common::stream::connection_socket> connection_socket_{};
        std::vector<std::unique_ptr<stream_worker>> workers_{};
    };

public:
    status initialize(api::environment& env, void* context) override {
        auto& ctx = *reinterpret_cast<stream_endpoint_context*>(context);  //NOLINT
        auto& options = ctx.options_;

        // create listener object
        listener_ = std::make_unique<listener>(env, std::stol(options["threads"]), std::stol(options["port"]));

        return status::ok;
    }

    status start() override {
        listener_thread_ = std::thread(std::ref(*listener_));
        return status::ok;
    }

    status shutdown() override {
        listener_->terminate();
        listener_thread_.join();
        return status::ok;
    }

    static std::shared_ptr<stream_provider> create() {
        return std::make_shared<stream_provider>();
    }

private:
    std::unique_ptr<listener> listener_;
    std::thread listener_thread_;
};

}  // tateyama::server
