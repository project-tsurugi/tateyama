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

// FIXME: temporary header handling - start
// copied from tsubakuro/modules/ipc/src/main/native/include/
// namespace is changed to tsubakuro::common::wire
#include <tsubakuro/common/wire/wire.h>
#include <tsubakuro/common/wire/udf_wires.h>
// FIXME: temporary header handling - end

#include "server_client_base.h"
#include "watch_dog.h"

namespace tateyama::api::endpoint::ipc {

class ipc_client {
public:
    explicit ipc_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg);
    void send(const std::size_t tag, const std::string &message);
    void receive(std::string &message);

    std::size_t session_id() const {
        return session_id_;
    }

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
