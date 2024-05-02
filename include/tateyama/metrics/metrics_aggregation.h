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
    std::string_view group_key() const noexcept {
        return group_key_;
    }

    /**
     * @brief returns the metrics item description.
     * @returns the metrics item description
     */
    std::string_view description() const noexcept {
        return description_;
    }

    /**
     * @brief returns a new aggregator of this aggregation.
     * @returns the aggregation operation type
     */
    virtual std::unique_ptr<metrics_aggregator> create_aggregator() const noexcept = 0;

    /**
     * @brief create a new object
     * @param group_key the metrics aggregation group key string
     * @param description the metrics item description
     */
    metrics_aggregation(
        const std::string& group_key,
        const std::string& description) noexcept :
        group_key_(group_key),
        description_(description) {
    }

private:
    std::string group_key_{};
    std::string description_{};
};

}
