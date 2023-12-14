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

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/routing_service.h>

#include "tateyama/endpoint/common/session_info_impl.h"

namespace tateyama::endpoint::common {

class worker_common {
public:
    worker_common(std::size_t id, std::string_view conn_type, std::string_view conn_info)
        : session_info_(id, conn_type, conn_info) {
    }
    worker_common(std::size_t id, std::string_view conn_type) : worker_common(id, conn_type, "") {
    }

protected:
    session_info_impl session_info_;  // NOLINT

    bool handshake(tateyama::api::server::request*, tateyama::api::server::response*) {
        return true;
    }
};

}  // tateyama::endpoint::common
