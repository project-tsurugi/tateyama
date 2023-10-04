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

#include "loopback_data_writer.h"

namespace tateyama::endpoint::loopback {

tateyama::status loopback_data_writer::write(const char *data, std::size_t length) {
    if (length > 0) {
        // NOTE: data is binary data. It maybe data="\0\1\2\3", length=4 etc.
        current_data_.append(data, length);
    }
    return tateyama::status::ok;
}

tateyama::status loopback_data_writer::commit() {
    if (current_data_.length() > 0) {
        committed_data_list_.emplace_back(std::move(current_data_));
        current_data_ = { };
    }
    return tateyama::status::ok;
}

std::vector<std::string> loopback_data_writer::release_committed_data() noexcept {
    std::vector<std::string> result { };
    committed_data_list_.swap(result);
    return result;
}

} // namespace tateyama::endpoint::loopback
