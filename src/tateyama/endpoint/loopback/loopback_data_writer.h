/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <vector>

#include <tateyama/api/server/writer.h>

namespace tateyama::endpoint::loopback {

/**
 * @brief data writer for loopback endpoint
 * @note this class is not designed as thread-safe
 */
class loopback_data_writer: public tateyama::api::server::writer {
public:
    tateyama::status write(const char *data, std::size_t length) override;
    tateyama::status commit() override;

    // just for unit test
    [[nodiscard]] std::vector<std::string> const& committed_data() const noexcept {
        return committed_data_list_;
    }

    [[nodiscard]] std::vector<std::string> release_committed_data() noexcept;
private:
    std::string current_data_ { };
    std::vector<std::string> committed_data_list_ { };
};

} // namespace tateyama::endpoint::loopback
