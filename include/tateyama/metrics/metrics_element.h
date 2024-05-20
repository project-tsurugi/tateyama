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

#include <string>
#include <vector>
#include <tuple>
#include <memory>

#include <tateyama/metrics/metrics_element.h>

namespace tateyama::metrics::resource {
class metrics_element_impl;
}

namespace tateyama::metrics {

/**
 * @brief represents an element of multi-value metrics item.
 */
class metrics_element {
public:
    /**
     * @brief returns the value of this element.
     * @returns the element value
     */
    [[nodiscard]] double value() const noexcept;

    /**
     * @brief returns the attributes of this item
     * @returns the attributes of this item
     */
    [[nodiscard]] std::vector<std::tuple<std::string, std::string>> const& attributes() const noexcept;

    explicit metrics_element(
        std::shared_ptr<tateyama::metrics::resource::metrics_element_impl> arg
    ) noexcept;

    ~metrics_element() = default;

    metrics_element(metrics_element const&) = delete;
    metrics_element(metrics_element&&) = delete;
    metrics_element& operator = (metrics_element const&) = delete;
    metrics_element& operator = (metrics_element&&) = delete;

private:
    std::shared_ptr<tateyama::metrics::resource::metrics_element_impl> body_;
};

}
