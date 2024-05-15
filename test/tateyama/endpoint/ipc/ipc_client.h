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

#include "ipc_test_utils.h"

// FIXME: temporary header handling - start
// copied from tsubakuro/modules/ipc/src/main/native/include/
// namespace is changed to tsubakuro::common::wire
#include <tsubakuro/common/wire/wire.h>
#include <tsubakuro/common/wire/udf_wires.h>
// FIXME: temporary header handling - end

#include "server_client_base.h"

namespace tateyama::endpoint::ipc {

using resultset_wires_container = tsubakuro::common::wire::session_wire_container::resultset_wires_container;

class ipc_client {
public:
    ipc_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, tateyama::proto::endpoint::request::Handshake& hs);
    explicit ipc_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg) : ipc_client(cfg, default_endpoint_handshake_) {
    }
    ipc_client(std::string_view database_name, std::size_t session_id, tateyama::proto::endpoint::request::Handshake& hs);
    ipc_client(std::string_view database_name, std::size_t session_id) : ipc_client(database_name, session_id, default_endpoint_handshake_) {
    }
    ~ipc_client() { disconnect(); }

    void send(const std::size_t tag, const std::string &message, std::size_t index_offset = 0);
    void receive(std::string &message);
    void receive(std::string &message, tateyama::proto::framework::response::Header::PayloadType& type);
    void disconnect() {
        if (!disconnected_) {
            request_wire_->disconnect();
            disconnected_ = true;
        }
    }
    resultset_wires_container* create_resultset_wires();
    void dispose_resultset_wires(resultset_wires_container *rwc);

    std::size_t session_id() const noexcept {
        return session_id_;
    }

    std::string session_name() const noexcept {
        return session_name_;
    }

    // tateyama/src/tateyama/endpoint/ipc/bootstrap/server_wires_impl.h
    // - private server_wire_container_impl::resultset_buffer_size = 64KB
    // tateyama/src/tateyama/endpoint/ipc/wire.h
    // - public tateyama::common::wire::length_header::length_type = std::uint32_t
    // - public tateyama::common::wire::length_header::size = sizeof(length_type)
    // record_max = 64 KB - 4
    static constexpr std::size_t resultset_record_maxlen = 64 * 1024 - tateyama::common::wire::length_header::size;

private:
    tateyama::proto::endpoint::request::Handshake& endpoint_handshake_;
    std::string database_name_ { };
    std::unique_ptr<tsubakuro::common::wire::connection_container> container_ { };
    std::size_t id_ { };
    std::size_t session_id_ { };
    std::string session_name_ { };
    std::unique_ptr<tsubakuro::common::wire::session_wire_container> swc_ { };
    tsubakuro::common::wire::session_wire_container::wire_container *request_wire_ { };
    tsubakuro::common::wire::session_wire_container::response_wire_container *response_wire_ { };
    tateyama::proto::endpoint::request::Handshake default_endpoint_handshake_{ };
    bool disconnected_{ };

    void handshake();
    void receive(std::string &message, tateyama::proto::framework::response::Header::PayloadType type, bool do_check);
};

} // namespace tateyama::endpoint::ipc
