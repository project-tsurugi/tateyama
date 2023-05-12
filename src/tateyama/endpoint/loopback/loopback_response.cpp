/*
 * Copyright 2018-2023 tsurugi project.
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

#include "loopback_response.h"

namespace tateyama::endpoint::loopback {

tateyama::status loopback_response::acquire_channel(std::string_view name,
        std::shared_ptr<tateyama::api::server::data_channel> &ch) {
    std::unique_lock<std::mutex> lock(mtx_channel_map_);
    if (channel_map_.find(name) != channel_map_.cend()) {
        // already acquired the same name channel
        return tateyama::status::not_found;
    }
    auto data_channel = std::make_shared<loopback_data_channel>(name);
    ch = data_channel;
    channel_map_.try_emplace(data_channel->name(), std::move(data_channel));
    return tateyama::status::ok;
}

tateyama::status loopback_response::release_channel(tateyama::api::server::data_channel &ch) {
    auto data_channel = dynamic_cast<loopback_data_channel*>(&ch);
    auto &name = data_channel->name();
    //
    {
        std::unique_lock<std::mutex> lock(mtx_channel_map_);
        auto it = channel_map_.find(name);
        if (it == channel_map_.cend() || it->second.get() != data_channel) {
            return tateyama::status::not_found;
        }
        channel_map_.erase(it);
    }
    {
        std::unique_lock<std::mutex> lock(mtx_committed_data_map_);
        committed_data_map_.try_emplace(name, data_channel->release_committed_data());
    }
    return tateyama::status::ok;
}

std::map<std::string, std::vector<std::string>, std::less<>> loopback_response::release_all_committed_data() noexcept {
    std::map<std::string, std::vector<std::string>, std::less<>> result { };
    committed_data_map_.swap(result);
    return result;
}

} // namespace tateyama::endpoint::loopback
