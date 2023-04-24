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

#include <algorithm>

#include "loopback_data_channel.h"

namespace tateyama::common::loopback {

tateyama::status loopback_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer> &writer) {
    auto wrt = std::make_shared<loopback_data_writer>();
    {
        std::unique_lock < std::shared_mutex > lock(mtx_writers_);
        writers_.emplace_back(wrt.get());
    }
    writer = wrt;
    return tateyama::status::ok;
}

tateyama::status loopback_data_channel::release(tateyama::api::server::writer &writer) {
    auto wrt = dynamic_cast<loopback_data_writer*>(&writer);
    {
        std::unique_lock < std::shared_mutex > lock(mtx_writers_);
        auto it = std::find(writers_.cbegin(), writers_.cend(), wrt);
        if (it == writers_.cend()) {
            return tateyama::status::not_found;
        }
        writers_.erase(it);
    }
    {
        std::unique_lock < std::shared_mutex > lock(mtx_released_data_);
        for (auto &data : wrt->committed_data()) {
            // data is moved to released_data_
            // because writer is released, and data in the writer is freed also.
            released_data_.emplace_back(std::move(data));
        }
    }
    return tateyama::status::ok;
}

void loopback_data_channel::release() {
    std::shared_lock < std::shared_mutex > lock(mtx_writers_);
    auto writers_copy { writers_ };
    lock.unlock();
    //
    for (auto &writer : writers_copy) {
        release(*writer);
    }
}

void loopback_data_channel::append_committed_data(std::vector<std::string> &whole) {
    {
        std::shared_lock < std::shared_mutex > lock(mtx_released_data_);
        for (auto &data : released_data_) {
            whole.emplace_back(data);
        }
    }
    {
        std::shared_lock < std::shared_mutex > lock(mtx_writers_);
        for (auto &writer : writers_) {
            for (auto &data : writer->committed_data()) {
                whole.emplace_back(data);
            }
        }
    }
}

} // namespace tateyama::common::loopback
