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

#include <string_view>
#include <set>
#include <map>
#include <utility>

#include <tateyama/api/server/request.h>

#include "tateyama/framework/component_ids.h"
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/api/server/blob_info.h>
#include <tateyama/endpoint/common/pointer_comp.h>
#include <tateyama/utils/protobuf_utils.h>

namespace tateyama::endpoint::common {

struct parse_result {
    std::string_view payload_{};
    std::size_t service_id_{};
    std::size_t session_id_{};
};

inline bool parse_header(std::string_view input, parse_result& result, [[maybe_unused]] std::map<std::string, std::pair<std::filesystem::path, bool>>& blobs_map) {
    result = {};
    ::tateyama::proto::framework::request::Header hdr{};
    google::protobuf::io::ArrayInputStream in{input.data(), static_cast<int>(input.size())};
    if(auto res = utils::ParseDelimitedFromZeroCopyStream(std::addressof(hdr), std::addressof(in), nullptr); ! res) {
        return false;
    }
    result.session_id_ = hdr.session_id();
    result.service_id_ = hdr.service_id();
    if(hdr.has_blobs()) {
        for(auto&& e : hdr. blobs().blobs()) {
            blobs_map.emplace(e.channel_name(), std::make_pair(e.path(), e.temporary()));
        }
    }
    return utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, result.payload_);
}

struct header_content {
    std::size_t session_id_{};
    std::set<std::unique_ptr<tateyama::api::server::blob_info>, pointer_comp<tateyama::api::server::blob_info>>* blobs_{};
};

inline bool append_response_header(std::stringstream& ss, std::string_view body, header_content input, ::tateyama::proto::framework::response::Header::PayloadType type = ::tateyama::proto::framework::response::Header::UNKNOWN) {
    ::tateyama::proto::framework::response::Header hdr{};
    hdr.set_session_id(input.session_id_);
    hdr.set_payload_type(type);
    if(input.blobs_ && type == ::tateyama::proto::framework::response::Header::SERVICE_RESULT) {
        if (!(input.blobs_)->empty()) {
            auto* blobs = hdr.mutable_blobs();
            for(auto&& e: *input.blobs_) {
                auto* blob = blobs->add_blobs();
                auto cn = e->channel_name();
                blob->set_channel_name(cn.data(), cn.length());
                blob->set_path((e->path()).string());
                blob->set_temporary(e->is_temporary());
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

}  // tateyama::endpoint::ipc
