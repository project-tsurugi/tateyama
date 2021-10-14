/*
 * Copyright 2019-2019 tsurugi project.
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
#include <setjmp.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "../common/utils/loader.h"
#include <tateyama/api/endpoint/service.h>
#include <jogasaki/api.h>

#include "worker.h"
#include "server.h"
#include "utils.h"

namespace tateyama::server {

class ipc_provider : public tateyama::api::endpoint::provider {
    tateyama::status initialize(void* context) override {
        // connection channel
        container_ = std::make_unique<tateyama::common::wire::connection_container>(FLAGS_dbname);

        // worker objects
        workers_.reserve(FLAGS_threads);

        int return_value{0};
        auto& connection_queue = container->get_connection_queue();
        while(true) {
            auto session_id = connection_queue.listen(true);
            if (connection_queue.is_terminated()) {
                VLOG(1) << "receive terminate request";
                workers.clear();
                connection_queue.confirm_terminated();
                break;
            }
            VLOG(1) << "connect request: " << session_id;
            std::string session_name = FLAGS_dbname;
            session_name += "-";
            session_name += std::to_string(session_id);
            auto wire = std::make_unique<tateyama::common::wire::server_wire_container_impl>(session_name);
            VLOG(1) << "created session wire: " << session_name;
            connection_queue.accept(session_id);
            std::size_t index;
            for (index = 0; index < workers.size() ; index++) {
                if (auto rv = workers.at(index)->future_.wait_for(std::chrono::seconds(0)) ; rv == std::future_status::ready) {
                    break;
                }
            }
            if (workers.size() < (index + 1)) {
                workers.resize(index + 1);
            }
            try {
                std::unique_ptr<Worker> &worker = workers.at(index);
                worker = std::make_unique<Worker>(*service, session_id, std::move(wire));
                worker->task_ = std::packaged_task<void()>([&]{worker->run();});
                worker->future_ = worker->task_.get_future();
                worker->thread_ = std::thread(std::move(worker->task_));
            } catch (std::exception &ex) {
                LOG(ERROR) << ex.what();
                return_value = -1;
                workers.clear();
                break;
            }
        }
    }

    tateyama::status shutdown() override {
        for (std::size_t index = 0; index < workers.size() ; index++) {
            if (auto rv = workers.at(index)->future_.wait_for(std::chrono::seconds(0)) ; rv != std::future_status::ready) {
                VLOG(1) << "exit: remaining thread " << workers.at(index)->session_id_;
            }
        }
        workers.clear();
    }

    static std::shared_ptr<ipc_provider> create() {
        return std::make_shared<ipc_provider>();
    }

private:
    std::make_unique<tateyama::common::wire::connection_container> container_{};
    std::vector<std::unique_ptr<Worker>> workers_{};
}

register_component(provider, ipc_endpoint, ipc_provider::create);

}  // tateyama::server