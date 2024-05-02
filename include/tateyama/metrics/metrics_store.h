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

#include <tateyama/metrics/metrics_item_slot.h>
#include <tateyama/metrics/metrics_aggregation.h>
#include <tateyama/metrics/metrics_metadata.h>

namespace tateyama::metrics::resource {
class metrics_store_impl;
}

namespace tateyama::metrics {

/**
 * @brief A storage that holds the latest value of individual metrics items.
 */
class metrics_store {
public:
    /**
     * @brief registers a new metrics item.
     * @param metadata metadata of metrics item to register
     * @returns the metrics item slot for the registered one
     * @throws std::runtime_error if another item with the same key is already in this store
     */
    metrics_item_slot& register_item(metrics_metadata metadata);
    // NOTE: or std::shared_ptr ?

    /**
     * @brief registers a new aggregation of metrics items.
     * @param aggregation aggregation specification to register
     * @throws std::runtime_error if another aggregation with the same key is already in this store
     */
    void register_aggregation(std::unique_ptr<metrics_aggregation> aggregation);

    /**
     * @brief removes the previously registered metrics item or aggregation.
     * @param key the metrics item key, or group key of metrics aggregation
     * @returns true if successfully the registered element
     * @returns false if no such the element in this store
     */
    bool unregister_element(std::string_view key);

    /**
     * @brief enumerates registered metrics items and their values.
     * @param acceptor the callback that accepts each metrics item and its value
     * @attention this operation may prevent from registering metrics items
     */
    void enumerate_items(std::function<void(metrics_metadata const&, double)> const& acceptor);

    /**
     * @brief enumerates registered metrics items of the specified value type.
     * @param acceptor the callback that accepts each metrics aggregation
     * @attention this operation may prevent from registering metrics aggregations
     */
    void enumerate_aggregations(std::function<void(metrics_aggregation const&)> const& acceptor) const;

    explicit metrics_store(
        std::unique_ptr<tateyama::metrics::resource::metrics_store_impl> arg
    ) noexcept;

private:
    std::unique_ptr<tateyama::metrics::resource::metrics_store_impl> body_;
};

}
