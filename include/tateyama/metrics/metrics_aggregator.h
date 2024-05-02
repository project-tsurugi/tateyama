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

#include <variant>
#include <vector>

#include <tateyama/metrics/metrics_element.h>
#include <tateyama/metrics/metrics_metadata.h>

namespace tateyama::metrics {

/**
 * @brief calculates aggregation of metrics items.
 */
class metrics_aggregator {
public:
    /// @brief the aggregation result type
    using result_type = std::variant<double, std::vector<metrics_element>>;

    /**
     * @brief creates a new instance.
     */
    constexpr metrics_aggregator() noexcept = default;

    /**
     * @brief disposes this instance.
     */
    virtual ~metrics_aggregator() = default;

    // NOTE: cannot copy, cannot move

    /**
     * @brief add a metrics item and its value.
     * @attention the specified item must be an aggregation target
     */
    virtual void add(metrics_metadata const& metadata, double value) = 0;

    /**
     * @brief aggregates the previously added items.
     * @returns the aggregation result
     */
    virtual result_type aggregate() = 0;
};

}
