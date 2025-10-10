/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include <chrono>

#include "tateyama/endpoint/common/worker_common.h"

#include "tateyama/endpoint/stream/stream.h"

namespace tateyama::endpoint::stream::bootstrap {

class alignas(64) stream_worker : public tateyama::endpoint::common::worker_common {
 public:
    stream_worker(tateyama::framework::routing_service& service,
                  const tateyama::endpoint::common::configuration& conf,
                  std::size_t session_id,
                  std::unique_ptr<stream_socket> stream,
                  const bool decline)
        : worker_common(conf, session_id, stream->connection_info()),
          service_(service),
          session_stream_(std::move(stream)),
          conf_(conf),
          decline_(decline) {
    }

    void run();
    bool terminate(tateyama::session::shutdown_request_type type);

 private:
    tateyama::framework::routing_service& service_;
    std::unique_ptr<stream_socket> session_stream_;
    const tateyama::endpoint::common::configuration& conf_;
    const bool decline_;
    static constexpr std::chrono::duration poll_interval = std::chrono::milliseconds(20);

    void notify_of_decline(tateyama::proto::endpoint::request::Request& rq, tateyama::api::server::response* response) {
        switch (rq.command_case()) {
        case tateyama::proto::endpoint::request::Request::kEncryptionKey:
        {
            tateyama::proto::endpoint::response::EncryptionKey rp{};
            auto re = rp.mutable_error();
            re->set_message("requests for session connections exceeded the maximum number of sessions");
            re->set_code(tateyama::proto::diagnostics::Code::RESOURCE_LIMIT_REACHED);
            response->body(rp.SerializeAsString());
            rp.clear_error();
            return;
        }
        case tateyama::proto::endpoint::request::Request::kHandshake:
        {
            tateyama::proto::endpoint::response::Handshake rp{};
            auto re = rp.mutable_error();
            re->set_message("requests for session connections exceeded the maximum number of sessions");
            re->set_code(tateyama::proto::diagnostics::Code::RESOURCE_LIMIT_REACHED);
            response->body(rp.SerializeAsString());
            rp.clear_error();
            return;
        }
        default:
            notify_client(response, tateyama::proto::diagnostics::Code::INVALID_REQUEST, "invalid request for handshake phase");
        }
    }

    void resultset_force_close() override {
    }
    bool has_incomplete_resultset() override {
        return false;
    }
};

}
