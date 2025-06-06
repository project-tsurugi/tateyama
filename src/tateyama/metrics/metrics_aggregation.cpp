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

#include <tateyama/metrics/metrics_aggregation.h>

namespace tateyama::metrics {

metrics_aggregation::metrics_aggregation(std::string_view group_key,
                                         std::string_view description,
                                         std::function<std::unique_ptr<metrics_aggregator>()> factory) noexcept :
    group_key_(group_key),
    description_(description),
    factory_(std::move(factory)) {
}

std::string_view metrics_aggregation::group_key() const noexcept {
    return group_key_;
}

std::string_view metrics_aggregation::description() const noexcept {
    return description_;
}

std::unique_ptr<metrics_aggregator> metrics_aggregation::create_aggregator() const {
    return factory_();
}

}
