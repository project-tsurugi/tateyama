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
#include <string_view>
#include <tuple>
#include <vector>

namespace tateyama::metrics {

/**
 * @brief represents metadata of metrics item.
 */
class metrics_metadata {
public:
    /**
     * @brief returns the metrics item key.
     * @returns the metrics item key string
     */
    [[nodiscard]] std::string_view key() const noexcept;

    /**
     * @brief returns the metrics item description.
     * @returns the metrics item description
     */
    [[nodiscard]] std::string_view description() const noexcept;

    /**
     * @brief returns the attributes of this item
     * @returns the attributes of this item
     */
    [[nodiscard]] std::vector<std::tuple<std::string, std::string>> const& attributes() const noexcept;

    /**
     * @brief returns whether to include this item to metrics.
     * @details For example, you can disable this feature to use this item only for aggregation, but do not display.
     * @returns true if this metrics item is visible
     * @returns false otherwise
     */
    [[nodiscard]] bool is_visible() const noexcept;

    /**
     * @brief returns key list of the aggregation group which this item participating.
     * @returns the metrics aggregation group keys
     * @return empty vector if this item is not a member of any groups
     */
    [[nodiscard]] std::vector<std::string> const& group_keys() const noexcept;

    /**
     * @brief creates a new instance.
     * @param key the metrics item key string
     * @param description the metrics item description
     * @param attributes the attributes of this item
     * @param group_keys the group keys
     * @param visible true if this metrics item is visible
     */
    metrics_metadata(
        std::string_view key,
        std::string_view description,
        std::vector<std::tuple<std::string, std::string>> attributes,
        std::vector<std::string> group_keys,
        bool visible = true) :
        key_(key),
        description_(description),
        attributes_(std::move(attributes)),
        group_keys_(std::move(group_keys)),
        visible_(visible) {
    }

private:
    const std::string key_;
    const std::string description_;
    const std::vector<std::tuple<std::string, std::string>> attributes_;
    const std::vector<std::string> group_keys_;
    bool visible_;
};

}
