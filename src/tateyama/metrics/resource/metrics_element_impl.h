/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <tateyama/metrics/metrics_element.h>

namespace tateyama::metrics::resource {

class metrics_element_impl {
public:
    metrics_element_impl() = default;

    double value() const noexcept { return value_; }

    std::vector<std::tuple<std::string, std::string>> const& attributes() const noexcept { return attributes_; }

private:
    double value_{};

    std::vector<std::tuple<std::string, std::string>> attributes_{};
};

}
