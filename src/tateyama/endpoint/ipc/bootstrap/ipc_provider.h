/*
 * Copyright 2019-2021 tsurugi project.
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

#include "worker.h"

namespace tateyama::server {

// should be in sync one in bootstrap
struct ipc_endpoint_context {
    std::unordered_map<std::string, std::string> options_{};
};

/**
 * @brief ipc endpoint provider
 * @details
 */
class ipc_provider : public tateyama::api::endpoint::provider {
private:
    class listener {
    public:
        listener(api::environment& env, std::size_t size, std::string& name) : env_(env), base_name_(name) {
            // connection channel
            container_ = std::make_unique<tateyama::common::wire::connection_container>(name);

            // worker objects
            workers_.reserve(size);
        }

        void operator()() {
            auto& connection_queue = container_->get_connection_queue();

            while(true) {
                auto session_id = connection_queue.listen(true);
                if (connection_queue.is_terminated()) {
                    VLOG(log_debug) << "receive terminate request";
                    for (auto & worker : workers_) {
                        if (auto rv = worker->future_.wait_for(std::chrono::seconds(0)) ; rv != std::future_status::ready) {
                            VLOG(log_debug) << "exit: remaining thread " << worker->session_id_;
                        }
                    }
                    workers_.clear();
                    connection_queue.confirm_terminated();
                    break;
                }
                VLOG(log_debug) << "connect request: " << session_id;
                std::string session_name = base_name_;
                session_name += "-";
                session_name += std::to_string(session_id);
                auto wire = std::make_unique<tateyama::common::wire::server_wire_container_impl>(session_name);
                VLOG(log_debug) << "created session wire: " << session_name;
                connection_queue.accept(session_id);
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
                    std::unique_ptr<Worker> &worker = workers_.at(index);
                    worker = std::make_unique<Worker>(*env_.endpoint_service(), session_id, std::move(wire));
                    worker->task_ = std::packaged_task<void()>([&]{worker->run();});
                    worker->future_ = worker->task_.get_future();
                    worker->thread_ = std::thread(std::move(worker->task_));
                } catch (std::exception &ex) {
                    LOG(ERROR) << ex.what();
                    workers_.clear();
                    break;
                }
            }
        }

        void terminate() {
            container_->get_connection_queue().request_terminate();
        }

    private:
        api::environment& env_;
        std::unique_ptr<tateyama::common::wire::connection_container> container_{};
        std::vector<std::unique_ptr<Worker>> workers_{};
        std::string base_name_;
    };

public:
    status initialize(api::environment& env, void* context) override {
        auto& ctx = *reinterpret_cast<ipc_endpoint_context*>(context);  //NOLINT
        auto& options = ctx.options_;

        // create listener object
        listener_ = std::make_unique<listener>(env, std::stol(options["threads"]), options["dbname"]);

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

    static std::shared_ptr<ipc_provider> create() {
        return std::make_shared<ipc_provider>();
    }

private:
    std::unique_ptr<listener> listener_;
    std::thread listener_thread_;
};

}  // tateyama::server
