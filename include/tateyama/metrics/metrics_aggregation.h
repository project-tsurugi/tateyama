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

#include <memory>
#include <string_view>
#include <functional>

#include <tateyama/metrics/metrics_aggregator.h>

namespace tateyama::metrics {

/**
 * @brief parent class of a class that aggregates metrics,
 * Actual aggregation class used is not this class itself, but a class that inherits from this class.
 */
class metrics_aggregation {
public:
    /**
     * @brief returns the metrics aggregation group key.
     * @returns the metrics aggregation group key string
     */
    [[nodiscard]] std::string_view group_key() const noexcept;

    /**
     * @brief returns the metrics item description.
     * @returns the metrics item description
     */
    [[nodiscard]] std::string_view description() const noexcept;

    /**
     * @brief returns a new aggregator of this aggregation.
     * @returns the aggregation operation type
     */
    [[nodiscard]] std::unique_ptr<metrics_aggregator> create_aggregator() const;

    /**
     * @brief create a new object
     * @param group_key the metrics aggregation group key string
     * @param description the metrics item description
     * @param factory a function to create the metrics_aggregator object
     */
    metrics_aggregation(
        std::string_view group_key,
        std::string_view description,
        std::function<std::unique_ptr<metrics_aggregator>()>&& factory) noexcept;

private:
    const std::string group_key_{};
    const std::string description_{};
    const std::function<std::unique_ptr<metrics_aggregator>()> factory_;
};

}
