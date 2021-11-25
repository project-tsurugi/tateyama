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

#include <tateyama/api/endpoint/service.h>
#include <tateyama/api/endpoint/provider.h>
#include <tateyama/api/registry.h>
#include <tateyama/api/environment.h>
#include <tateyama/utils/thread_affinity.h>

#include "worker.h"

namespace tateyama::server {

// should be in sync one in bootstrap
struct ipc_endpoint_context {
    std::unordered_map<std::string, std::string> options_{};
    std::function<void()> database_initialize_{};
};

/**
 * @brief ipc endpoint provider
 * @details
 */
class ipc_provider : public tateyama::api::endpoint::provider {
private:
    class listener {
    public:
        listener(api::environment& env, std::size_t size, std::string& name, utils::affinity_profile profile) :
            env_(env),
            base_name_(name),
            profile_(std::move(profile))
        {
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
                    VLOG(1) << "receive terminate request";
                    for (auto & worker : workers_) {
                        if (auto rv = worker->future_.wait_for(std::chrono::seconds(0)) ; rv != std::future_status::ready) {
                            VLOG(1) << "exit: remaining thread " << worker->session_id_;
                        }
                    }
                    workers_.clear();
                    connection_queue.confirm_terminated();
                    break;
                }
                VLOG(1) << "connect request: " << session_id;
                std::string session_name = base_name_;
                session_name += "-";
                session_name += std::to_string(session_id);
                auto wire = std::make_unique<tateyama::common::wire::server_wire_container_impl>(session_name);
                VLOG(1) << "created session wire: " << session_name;
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
                    worker->task_ = std::packaged_task<void()>([&]{
                        utils::set_thread_affinity(index, profile_);
                        worker->run();
                    });
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
        utils::affinity_profile profile_{};
    };

public:
    status initialize(api::environment& env, void* context) override {
        auto& ctx = *reinterpret_cast<ipc_endpoint_context*>(context);  //NOLINT
        auto& options = ctx.options_;
        auto& dbinit = ctx.database_initialize_;

        // create listener object
        listener_ = std::make_unique<listener>(
            env,
            std::stol(options["threads"]),
            options["dbname"],
            setup_affinity(
                std::stol(options["core_affinity"]) == 1,
                std::stol(options["assign_numa_nodes_uniformly"]) == 1,
                std::stoul(options["numa_node"]),
                std::stoul(options["initial_core"])
            )
        );

        // callbak jogasaki to load data
        dbinit();

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

    utils::affinity_profile setup_affinity(
        bool set_core_affinity,
        bool assign_numa_nodes_uniformly,
        std::size_t numa_node,
        std::size_t initial_core
    ) {
        utils::affinity_profile prof{};
        if(numa_node != utils::affinity_profile::npos) {
            prof = utils::affinity_profile{
                utils::affinity_tag<utils::affinity_kind::numa_affinity>,
                numa_node
            };
        } else if (assign_numa_nodes_uniformly) {
            prof = utils::affinity_profile{
                utils::affinity_tag<utils::affinity_kind::numa_affinity>
            };
        } else if(set_core_affinity) {
            prof = utils::affinity_profile{
                utils::affinity_tag<utils::affinity_kind::core_affinity>,
                initial_core
            };
        }
        return prof;
    }
private:
    std::unique_ptr<listener> listener_;
    std::thread listener_thread_;
};

}  // tateyama::server
