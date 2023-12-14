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

#include <future>
#include <thread>

#include "tateyama/endpoint/common/worker_common.h"

#include <tateyama/endpoint/stream/stream.h>
#include "tateyama/status/resource/database_info_impl.h"  // FIXME

namespace tateyama::server {
class stream_provider;

class stream_worker : public tateyama::endpoint::common::worker_common {
 public:
    stream_worker(tateyama::framework::routing_service& service,
                  std::size_t session_id,
                  std::shared_ptr<tateyama::common::stream::stream_socket> stream)
        : worker_common(session_id, "tcp/ip", stream->connection_info()),
          service_(service),
          session_stream_(std::move(stream)),
          session_id_(session_id) {
    }
    ~stream_worker() {
        if(thread_.joinable()) thread_.join();
    }

    /**
     * @brief Copy and move constructers are delete.
     */
    stream_worker(stream_worker const&) = delete;
    stream_worker(stream_worker&&) = delete;
    stream_worker& operator = (stream_worker const&) = delete;
    stream_worker& operator = (stream_worker&&) = delete;

    void run();
    friend class stream_listener;
    friend class stream_provider;

 private:
    tateyama::framework::routing_service& service_;
    std::shared_ptr<tateyama::common::stream::stream_socket> session_stream_;
    std::size_t session_id_;

    // for future
    std::packaged_task<void()> task_;
    std::future<void> future_;
    std::thread thread_{};

    // for step by step check
    const tateyama::status_info::resource::database_info_impl database_info_{};
};

}  // tateyama::server
