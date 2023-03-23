/*
 * Copyright 2018-2023 tsurugi project.
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
#pragma

#include "ipc_test_utils.h"

// FIXME: temporary header handling - start
// copied from tsubakuro/modules/ipc/src/main/native/include/
// namespace is changed to tsubakuro::common::wire
#include <tsubakuro/common/wire/wire.h>
#include <tsubakuro/common/wire/udf_wires.h>
// FIXME: temporary header handling - end

namespace tateyama::api::endpoint::ipc {

using resultset_wires_container = tsubakuro::common::wire::session_wire_container::resultset_wires_container;

class ipc_client {
public:
    explicit ipc_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg);
    void send(const std::size_t tag, const std::string &message);
    void receive(std::string &message);

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
    std::string database_name_ { };
    std::unique_ptr<tsubakuro::common::wire::connection_container> container_ { };
    std::size_t id_ { };
    std::size_t session_id_ { };
    std::string session_name_ { };
    std::unique_ptr<tsubakuro::common::wire::session_wire_container> swc_ { };
    tsubakuro::common::wire::session_wire_container::wire_container *request_wire_ { };
    tsubakuro::common::wire::session_wire_container::response_wire_container *response_wire_ { };
};

} // namespace tateyama::api::endpoint::ipc
