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
#include <tateyama/api/server/response.h>

#include <tateyama/api/endpoint/response.h>

namespace tateyama::api::server {

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
    return origin_->body_head(body_head);
}

status response::body(std::string_view body) {
    return origin_->body(body);
}

status response::release_channel(data_channel& ch) {
    return origin_->release_channel(*ch.origin());
}

status response::acquire_channel(std::string_view name, std::shared_ptr<data_channel>& ch) {
    endpoint::data_channel* c{};
    auto ret = origin_->acquire_channel(name, c);
    ch = std::make_shared<data_channel>(*c);
    return ret;
}

}
