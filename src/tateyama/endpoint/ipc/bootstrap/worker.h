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

#include <atomic>

#include "tateyama/endpoint/common/worker_common.h"

#include "server_wires_impl.h"

namespace tateyama::endpoint::ipc::bootstrap {
class ipc_provider;

class Worker : public tateyama::endpoint::common::worker_common {
 public:
    Worker(tateyama::framework::routing_service& service,
           std::size_t session_id,
           std::shared_ptr<server_wire_container_impl> wire,
           const tateyama::api::server::database_info& database_info)
        : worker_common(connection_type::ipc, session_id),
          service_(service),
          wire_(std::move(wire)),
          request_wire_container_(dynamic_cast<server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire())),
          database_info_(database_info) {
    }
    ~Worker() {
        if(thread_.joinable()) thread_.join();
    }

    /**
     * @brief Copy and move constructers are delete.
     */
    Worker(Worker const&) = delete;
    Worker(Worker&&) = delete;
    Worker& operator = (Worker const&) = delete;
    Worker& operator = (Worker&&) = delete;

    void run();
    void terminate();
    [[nodiscard]] std::size_t session_id() const noexcept { return session_id_; }
    [[nodiscard]] bool terminated() const {
        return wait_for() == std::future_status::ready;
    }

    friend class ipc_provider;

 private:
    tateyama::framework::routing_service& service_;
    std::shared_ptr<server_wire_container_impl> wire_;
    server_wire_container_impl::wire_container_impl* request_wire_container_;
    const tateyama::api::server::database_info& database_info_;
};

}
