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

#include <functional>
#include <utility>
#include <tateyama/metrics/metrics_item_slot.h>

namespace tateyama::metrics::resource {

class metrics_item_slot_impl {
public:
    metrics_item_slot_impl() = default;
    explicit metrics_item_slot_impl(std::function<double()> reader) : reader_(std::move(reader)) {}

    void set(double value) {
        value_ = value;
    }

    [[nodiscard]] double value() const {
        return reader_ ? reader_() : value_;
    }

private:
    double value_{};
    std::function<double()> reader_{};
};

}
