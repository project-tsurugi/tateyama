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
#include "response.h"

#include <takatori/util/assertion.h>

#include <tateyama/logging.h>
#include <tateyama/api/endpoint/response.h>
#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/response.pb.h>
#include <glog/logging.h>

namespace tateyama::api::server::impl {

std::shared_ptr<api::endpoint::response> const& response::origin() const noexcept {
    return origin_;
}

void response::code(response_code code) {
    origin_->code(code);
}

status response::close_session() {
    return origin_->close_session();
}

status response::body_head(std::string_view body_head) {
    if(session_id_ == unknown) {
        // legacy implementation
        // TODO remove
        return origin_->body_head(body_head);
    }
    std::stringstream ss{};
    if(auto res = append_header(ss, body_head); ! res) {
        VLOG(log_error) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    return origin_->body_head(s);
}

bool response::append_header(std::stringstream& ss, std::string_view body) const {
    ::tateyama::proto::framework::response::Header hdr{};
    BOOST_ASSERT(session_id_ != unknown); //NOLINT
    hdr.set_session_id(session_id_);
    if(auto res = utils::SerializeDelimitedToOstream(hdr, std::addressof(ss)); ! res) {
        return false;
    }

    if(auto res = utils::PutDelimitedBodyToOstream(body, std::addressof(ss)); ! res) {
        return false;
    }
    return true;
}

status response::body(std::string_view body) {
    if(session_id_ == unknown) {
        // legacy implementation
        // TODO remove
        return origin_->body(body);
    }
    std::stringstream ss{};
    if(auto res = append_header(ss, body); ! res) {
        VLOG(log_error) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    return origin_->body(s);
}

status response::release_channel(server::data_channel& ch) {
    auto& c = static_cast<server::impl::data_channel&>(ch);
    return origin_->release_channel(*c.origin());
}

status response::acquire_channel(std::string_view name, std::shared_ptr<server::data_channel>& ch) {
    endpoint::data_channel* c{};
    auto ret = origin_->acquire_channel(name, c);
    ch = std::make_shared<data_channel>(*c);
    return ret;
}

response::response(std::shared_ptr<api::endpoint::response> origin) :
    origin_(std::move(origin))
{}

void response::session_id(std::size_t id) {
    session_id_ = id;
}

}
