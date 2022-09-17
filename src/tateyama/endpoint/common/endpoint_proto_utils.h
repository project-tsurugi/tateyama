/*
 * Copyright 2019-2021 tsurugi project.
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

#include <tateyama/api/server/request.h>

#include "tateyama/framework/component_ids.h"
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/utils/protobuf_utils.h>

namespace tateyama::endpoint::common {

struct parse_result {
    std::string_view payload_{};
    std::size_t service_id_{};
    std::size_t session_id_{};
};

inline bool parse_header(std::string_view input, parse_result& result) {
    result = {};
    ::tateyama::proto::framework::request::Header hdr{};
    google::protobuf::io::ArrayInputStream in{input.data(), static_cast<int>(input.size())};
    if(auto res = utils::ParseDelimitedFromZeroCopyStream(std::addressof(hdr), std::addressof(in), nullptr); ! res) {
        return false;
    }
    result.session_id_ = hdr.session_id();
    result.service_id_ = hdr.service_id();
    return utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, result.payload_);
}

struct header_content {
    std::size_t session_id_{};
};

inline bool append_response_header(std::stringstream& ss, std::string_view body, header_content input) {
    ::tateyama::proto::framework::response::Header hdr{};
    hdr.set_session_id(input.session_id_);
    if(auto res = utils::SerializeDelimitedToOstream(hdr, std::addressof(ss)); ! res) {
        return false;
    }
    if(auto res = utils::PutDelimitedBodyToOstream(body, std::addressof(ss)); ! res) {
        return false;
    }
    return true;
}

}  // tateyama::endpoint::ipc
