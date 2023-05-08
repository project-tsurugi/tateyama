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

    [[nodiscard]] const std::vector<std::string>& committed_data() const noexcept {
        return list_;
    }
private:
    std::string current_data_ { };
    std::vector<std::string> list_ { };
};

} // namespace tateyama::endpoint::loopback
