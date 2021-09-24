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
#include <tateyama/api/server/data_channel.h>

#include <tateyama/api/endpoint/data_channel.h>
#include <tateyama/api/endpoint/writer.h>

namespace tateyama::api::server {

data_channel::data_channel(std::shared_ptr<api::endpoint::data_channel> origin) :
    origin_(std::move(origin))
{}

status data_channel::acquire(writer*& wrt) {
    api::endpoint::writer* w{};
    auto ret = origin_->acquire(w);
    writers_.emplace_back()

    *wrt.origin()
    return ret;
}

status data_channel::release(writer& wrt) {
    return origin_->release(*wrt.origin());
}

std::shared_ptr<api::endpoint::data_channel> const& data_channel::origin() const noexcept {
    return origin_;
}

}