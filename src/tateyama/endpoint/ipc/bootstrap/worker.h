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
#pragma once

#include <future>
#include <thread>

#include <tateyama/status.h>
#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>
#include <tateyama/api/endpoint/service.h>
#include <tateyama/api/endpoint/service.h>

#include "server_wires_impl.h"

namespace tateyama::bootstrap {
class ipc_provider;
}

namespace tateyama::server {

class Worker {
 public:
    Worker(tateyama::api::endpoint::service& service, std::size_t session_id, std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire)
        : service_(service), wire_(std::move(wire)),
          request_wire_container_(static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire())),
          session_id_(session_id) {
    }
    ~Worker() {
        if(thread_.joinable()) thread_.join();
    }
    void run();
    friend class ipc_provider;

 private:
    tateyama::api::endpoint::service& service_;
    std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire_;
    tateyama::common::wire::server_wire_container_impl::wire_container_impl* request_wire_container_;
    std::size_t session_id_;

    // for future
    std::packaged_task<void()> task_;
    std::future<void> future_;
    std::thread thread_{};
};

}  // tateyama::server
