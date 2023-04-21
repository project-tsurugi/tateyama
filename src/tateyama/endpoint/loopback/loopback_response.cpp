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

namespace tateyama::common::loopback {

bool loopback_response::has_channel(std::string_view name) noexcept {
    std::shared_lock < std::shared_timed_mutex > lock(mtx_channel_map_);
    return has_channel_nolock(name);
}

tateyama::status loopback_response::acquire_channel(std::string_view name,
        std::shared_ptr<tateyama::api::server::data_channel> &ch) {
    std::unique_lock < std::shared_timed_mutex > lock(mtx_channel_map_);
    if (has_channel_nolock(name)) {
        return tateyama::status::not_found;
    }
    ch = std::make_shared < loopback_data_channel > (name);
    std::string namestr { name };
    acquired_channel_map_[namestr] = dynamic_cast<loopback_data_channel*>(ch.get());
    //
    if (released_data_map_.find(namestr) == released_data_map_.cend()) {
        // NOTE: do not clear if acquire_channel() called with the same name again
        released_data_map_[namestr] = std::vector<std::string> { };
    }
    return tateyama::status::ok;
}

tateyama::status loopback_response::release_channel(tateyama::api::server::data_channel &ch) {
    auto data_channel = dynamic_cast<loopback_data_channel*>(&ch);
    auto name = data_channel->name();
    //
    std::unique_lock < std::shared_timed_mutex > lock(mtx_channel_map_);
    if (!has_channel_nolock(name)) {
        return tateyama::status::not_found;
    }
    if (acquired_channel_map_[name] != data_channel) {
        // it has same name, but it maybe another session's channel
        return tateyama::status::not_found;
    }
    acquired_channel_map_.erase(name);
    //
    std::vector < std::string > &whole = released_data_map_[name];
    data_channel->append_committed_data(whole);
    return tateyama::status::ok;
}

void loopback_response::all_committed_data(std::map<std::string, std::vector<std::string>> &data_map) {
    std::shared_lock < std::shared_timed_mutex > lock(mtx_channel_map_);
    for (const auto& [name, committed_data] : released_data_map_) {
        data_map[name] = committed_data;
    }
    for (const auto& [name, data_channel] : acquired_channel_map_) {
        std::vector < std::string > &vec = data_map[name];
        data_channel->append_committed_data(vec);
    }
}

} // namespace tateyama::common::loopback
