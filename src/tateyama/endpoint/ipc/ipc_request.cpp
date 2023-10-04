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
#include <glog/logging.h>

#include <tateyama/logging.h>

#include "ipc_request.h"

namespace tateyama::common::wire {

std::string_view
ipc_request::payload() const {
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_) << " length = " << payload_.length();  //NOLINT
    return payload_;
}

void
ipc_request::dispose() {
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_);  //NOLINT
}

std::size_t ipc_request::session_id() const {
    return session_id_;
}

std::size_t ipc_request::service_id() const {
    return service_id_;
}

}  // tateyama::common::wire
