/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <sstream>
#include <set>
#include <string_view>
#include <string>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/endpoint/common/endpoint_proto_utils.h>
#include <tateyama/proto/framework/common.pb.h>
#include <tateyama/proto/framework/request.pb.h>

namespace tateyama::endpoint {

struct request_header_content {
    std::size_t session_id_{};
    std::size_t service_id_{};
    std::set<std::tuple<std::string, std::string, bool>>* blobs_{};
};

inline bool append_request_header(std::stringstream& ss, std::string_view body, request_header_content input) {
    ::tateyama::proto::framework::request::Header hdr{};
    hdr.set_session_id(input.session_id_);
    hdr.set_service_id(input.service_id_);
    if (input.blobs_) {
        if (!(input.blobs_)->empty()) {
            ::tateyama::proto::framework::common::RepeatedBlobInfo blobs{};
            auto* mutable_blobs = hdr.mutable_blobs();
            for (auto&& e: *input.blobs_) {
                auto* mutable_blob = mutable_blobs->add_blobs();
                mutable_blob->set_channel_name(std::get<0>(e));
                mutable_blob->set_path(std::get<1>(e));
                mutable_blob->set_temporary(std::get<2>(e));
            }
        }
    }
    if(auto res = utils::SerializeDelimitedToOstream(hdr, std::addressof(ss)); ! res) {
        return false;
    }
    if(auto res = utils::PutDelimitedBodyToOstream(body, std::addressof(ss)); ! res) {
        return false;
    }
    return true;
}

}  // namespace tateyama::endpoint::ipc
