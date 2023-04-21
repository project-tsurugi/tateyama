/*
 * Copyright 2019-2023 tsurugi project.
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
#include <functional>
#include <iostream>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/status/resource/bridge.h>

#include "server_wires_impl.h"

namespace tateyama::server {
class ipc_provider;

class Worker {
 public:
    Worker(tateyama::framework::routing_service& service, std::size_t session_id, std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire, std::function<void(void)> clean_up)
        : service_(service), wire_(std::move(wire)),
          request_wire_container_(dynamic_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire())),
          session_id_(session_id),
          clean_up_(std::move(clean_up)) {
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

    friend class ipc_listener;
    friend class ipc_provider;

 private:
    tateyama::framework::routing_service& service_;
    std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire_;
    tateyama::common::wire::server_wire_container_impl::wire_container_impl* request_wire_container_;
    std::size_t session_id_;
    std::function<void(void)> clean_up_;

    // for future
    std::packaged_task<void()> task_;
    std::future<void> future_;
    std::thread thread_{};

    // for measurement
    std::chrono::time_point<std::chrono::steady_clock> time_{std::chrono::steady_clock::time_point()};
    std::chrono::nanoseconds duration_{};
    std::size_t count_{};

    void mark_begin() {
        time_ = std::chrono::steady_clock::now();
    }
    void mark_end() {
        if (time_ != std::chrono::steady_clock::time_point()) {
            auto now = std::chrono::steady_clock::now();
            duration_ += std::chrono::duration_cast<std::chrono::nanoseconds>(now - time_);
            time_ = std::chrono::steady_clock::time_point();
            count_++;
        }
    }
    std::size_t count() {
        return count_;
    }
    auto duration() {
        return duration_;
    }
    void print_out() {
        std::cout << count_ << " : " << (duration_.count() + 500) / 1000 << std::endl;
    }
};

}  // tateyama::server
