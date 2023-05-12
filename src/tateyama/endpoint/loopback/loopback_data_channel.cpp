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

#include "loopback_data_channel.h"

namespace tateyama::endpoint::loopback {

tateyama::status loopback_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer> &writer) {
    auto wrt = std::make_shared<loopback_data_writer>();
    writer = wrt;
    {
        std::lock_guard<std::mutex> lock(mtx_writers_);
        writers_.emplace_back(std::move(wrt));
    }
    return tateyama::status::ok;
}

tateyama::status loopback_data_channel::release(tateyama::api::server::writer &writer) {
    tateyama::status result = tateyama::status::not_found;
    const auto wrt = dynamic_cast<loopback_data_writer*>(&writer);
    {
        std::lock_guard<std::mutex> lock(mtx_writers_);
        for (auto it = writers_.cbegin(); it != writers_.cend(); it++) {
            if (it->get() == wrt) {
                writers_.erase(it);
                result = tateyama::status::ok;
                break;
            }
        }
    }
    if (result != tateyama::status::ok) {
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(mtx_committed_data_list_);
        auto list = wrt->release_committed_data();
        committed_data_list_.reserve(committed_data_list_.size() + list.size());
        committed_data_list_.insert(committed_data_list_.end(), std::make_move_iterator(list.begin()),
                std::make_move_iterator(list.end()));
    }
    return result;
}

std::vector<std::string> loopback_data_channel::release_committed_data() noexcept {
    std::vector<std::string> result { };
    committed_data_list_.swap(result);
    return result;
}

} // namespace tateyama::endpoint::loopback
