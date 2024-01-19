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

#include "tateyama/endpoint/stream/stream.h"

namespace tateyama::endpoint::stream::bootstrap {
class stream_provider;

class stream_worker : public tateyama::endpoint::common::worker_common {
 public:
    stream_worker(tateyama::framework::routing_service& service,
                  std::size_t session_id,
                  std::shared_ptr<stream_socket> stream,
                  const tateyama::api::server::database_info& database_info, bool decline)
        : worker_common(connection_type::stream, session_id, stream->connection_info()),
          service_(service),
          session_stream_(std::move(stream)),
          database_info_(database_info),
          decline_(decline) {
    }
    stream_worker(tateyama::framework::routing_service& service,
                  std::size_t session_id,
                  std::shared_ptr<stream_socket> stream,
                  const tateyama::api::server::database_info& database_info)
        : stream_worker(service, session_id, std::move(stream), database_info, false) {
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

    void run(const std::function<void(void)>& clean_up = [](){});
    friend class stream_provider;

 private:
    tateyama::framework::routing_service& service_;
    std::shared_ptr<stream_socket> session_stream_;
    const tateyama::api::server::database_info& database_info_;
    const bool decline_;

    void notify_of_decline(tateyama::api::server::response* response) {
        tateyama::proto::endpoint::response::Handshake rp{};
        auto rs = rp.mutable_error();
        rs->set_code(tateyama::proto::diagnostics::Code::RESOURCE_LIMIT_REACHED);
        response->body(rp.SerializeAsString());
        rp.clear_error();
    }
};

}
