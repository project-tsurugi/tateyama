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
#include <glog/logging.h>

#include <tateyama/logging.h>

#include "ipc_request.h"
#include "../common/endpoint_proto_utils.h"

namespace tateyama::common::wire {

server_wire_container&
ipc_request::get_server_wire_container() {
    return server_wire_;
}

std::string_view
ipc_request::payload() const {
    VLOG(log_trace) << __func__ << ": payload = \"" << payload_ << "\"" << std::endl;  //NOLINT
    return payload_;
}

void
ipc_request::dispose() {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT
}

std::size_t ipc_request::session_id() const {
    VLOG(log_trace) << __func__ << ": session_id = " << session_id_ << std::endl;  //NOLINT
    return session_id_;
}

std::size_t ipc_request::service_id() const {
    VLOG(log_trace) << __func__ << ": service_id = " << service_id_ << std::endl;  //NOLINT
    return service_id_;
}

}  // tateyama::common::wire
