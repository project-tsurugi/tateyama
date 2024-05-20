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
#include <cstdint>

namespace tateyama::metrics::resource {
class metrics_item_slot_impl;
}

namespace tateyama::metrics {

/**
 * @brief the metrics item slot to accept metrics item values.
 */
class metrics_item_slot {
public:
    /**
     * @brief notifies the latest value of metrics item.
     * @param value the latest value
     */
    void set(double value);

    /// @copydoc set()
    metrics_item_slot& operator=(double value);

    // NOTE: can not copy, can move
    metrics_item_slot(metrics_item_slot const&) = delete;
    metrics_item_slot(metrics_item_slot&&) = default;
    metrics_item_slot& operator = (metrics_item_slot const&) = delete;
    metrics_item_slot& operator = (metrics_item_slot&&) = default;

    explicit metrics_item_slot(
        std::shared_ptr<tateyama::metrics::resource::metrics_item_slot_impl> arg
    ) noexcept;

    ~metrics_item_slot() = default;

private:
    std::shared_ptr<tateyama::metrics::resource::metrics_item_slot_impl> body_;
};

}
