/*
 * Copyright 2019-2023 tsurugi project.
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

#include <mutex>
#include <shared_mutex>
#include <vector>

#include <tateyama/api/server/data_channel.h>

#include "loopback_data_writer.h"

namespace tateyama::common::loopback {

class loopback_data_channel: public tateyama::api::server::data_channel {
public:
    explicit loopback_data_channel(std::string_view name) :
            name_(name) {
    }
    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer> &writer) override;
    tateyama::status release(tateyama::api::server::writer &writer) override;

    /**
     * @brief release all unreleased writers if exist
     */
    void release();

    void append_committed_data(std::vector<std::string> &whole);
private:
    const std::string name_;

    std::shared_mutex mtx_writers_ { };
    std::vector<loopback_data_writer*> writers_ { };

    std::shared_mutex mtx_released_data_ { };
    std::vector<std::string> released_data_ { };
};

} // namespace tateyama::common::loopback