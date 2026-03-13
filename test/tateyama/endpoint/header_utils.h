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

class request_header_content {
public:
    constexpr static std::uint64_t FRAMEWORK_SERVICE_MESSAGE_VERSION_MAJOR = 0;
    constexpr static std::uint64_t FRAMEWORK_SERVICE_MESSAGE_VERSION_MINOR = 3;

    request_header_content()
        : session_id_(0), service_id_(0), blobs_(nullptr), grpc_blobs_(nullptr) {}
    request_header_content(std::size_t session_id, std::size_t service_id)
        : session_id_(session_id), service_id_(service_id), blobs_(nullptr), grpc_blobs_(nullptr) {}
    request_header_content(std::size_t session_id, std::size_t service_id, std::set<std::tuple<std::string, std::string, bool>>* blobs)
        : session_id_(session_id), service_id_(service_id), blobs_(blobs), grpc_blobs_(nullptr) {}
    request_header_content(std::size_t session_id, std::size_t service_id, std::set<std::tuple<std::string, std::uint64_t, std::uint64_t, bool>>* grpc_blobs)
        : session_id_(session_id), service_id_(service_id), blobs_(nullptr), grpc_blobs_(grpc_blobs) {}

    std::size_t session_id_{};
    std::size_t service_id_{};
    std::set<std::tuple<std::string, std::string, bool>>* blobs_{};
    std::set<std::tuple<std::string, std::uint64_t, std::uint64_t, bool>>* grpc_blobs_{};
};

inline bool append_request_header(std::stringstream& ss, std::string_view body, request_header_content input) {
    ::tateyama::proto::framework::request::Header hdr{};
    hdr.set_service_message_version_major(request_header_content::FRAMEWORK_SERVICE_MESSAGE_VERSION_MAJOR);
    hdr.set_service_message_version_minor(request_header_content::FRAMEWORK_SERVICE_MESSAGE_VERSION_MINOR);
    hdr.set_session_id(input.session_id_);
    hdr.set_service_id(input.service_id_);
    if (input.blobs_) {
        if (!(input.blobs_)->empty()) {
            auto* mutable_blobs = hdr.mutable_blobs();
            for (auto&& e: *input.blobs_) {
                auto* mutable_blob = mutable_blobs->add_blobs();
                mutable_blob->set_channel_name(std::get<0>(e));
                mutable_blob->set_path(std::get<1>(e));
                mutable_blob->set_temporary(std::get<2>(e));
            }
        }
    }
    if (input.grpc_blobs_) {
        if (!(input.grpc_blobs_)->empty()) {
            auto* mutable_blobs = hdr.mutable_blobs();
            for (auto&& e: *input.grpc_blobs_) {
                auto* mutable_blob = mutable_blobs->add_blobs();
                mutable_blob->set_channel_name(std::get<0>(e));
                auto* blob = mutable_blob->mutable_blob();
                blob->set_object_id(std::get<1>(e));
                blob->set_tag(std::get<2>(e));
                mutable_blob->set_temporary(std::get<3>(e));
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
