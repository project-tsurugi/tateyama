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
#include "request.h"

#include <google/protobuf/message_lite.h>

#include <tateyama/api/endpoint/request.h>
#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/framework/ids.h>

namespace tateyama::api::server::impl {

std::string_view request::payload() const {
    return payload_;
}

std::shared_ptr<api::endpoint::request const> const& request::origin() const noexcept {
    return origin_;
}

request::request(std::shared_ptr<api::endpoint::request const> origin) :
    origin_(std::move(origin))
{}

bool request::init() {
    ::tateyama::proto::framework::request::Header hdr{};
    auto pl = origin_->payload();
    google::protobuf::io::ArrayInputStream in{pl.data(), static_cast<int>(pl.size())};
    if(auto res = utils::ParseDelimitedFromZeroCopyStream(std::addressof(hdr), std::addressof(in), nullptr); ! res) {
        //return false;
        // for compatibility with request without header,
        service_id_ = tateyama::framework::service_id_sql;
        payload_ = pl;
        return true;
    }
    session_id_ = hdr.session_id();
    service_id_ = hdr.service_id();
    if(auto res = utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, payload_); ! res) {
        return false;
    }
    return true;
}

std::size_t request::session_id() const {
    return session_id_;
}

std::size_t request::service_id() const {
    return service_id_;
}

std::shared_ptr<api::server::request> create_request(std::shared_ptr<api::endpoint::request const> origin) {
    auto ret = std::make_shared<api::server::impl::request>(std::move(origin));
    if(auto res = ret->init(); ! res) {
        return {};
    }
    return ret;
}

}
