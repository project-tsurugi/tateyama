/*
 * Copyright 2018-2021 tsurugi project.
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
#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>
#include <tateyama/api/endpoint/service.h>
#include <tateyama/endpoint/common/endpoint_proto_utils.h>

#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

#include "server_wires_impl.h"

#include <gtest/gtest.h>

namespace tateyama::api::endpoint::ipc {

struct request_header_content {
    std::size_t session_id_{};
    std::size_t service_id_{};
};

inline bool append_request_header(std::stringstream& ss, std::string_view body, request_header_content input) {
    ::tateyama::proto::framework::request::Header hdr{};
    hdr.set_session_id(input.session_id_);
    hdr.set_service_id(input.service_id_);
    if(auto res = utils::SerializeDelimitedToOstream(hdr, std::addressof(ss)); ! res) {
        return false;
    }
    if(auto res = utils::PutDelimitedBodyToOstream(body, std::addressof(ss)); ! res) {
        return false;
    }
    return true;
}

}  // namespace tateyama::api::endpoint::ipc
