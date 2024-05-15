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

#include "resource/metrics_store_impl.h"

namespace tateyama::metrics {

metrics_store::metrics_store(std::unique_ptr<tateyama::metrics::resource::metrics_store_impl> arg) noexcept : body_(std::move(arg)) {
}

metrics_item_slot& metrics_store::register_item(const metrics_metadata& metadata) {
    return body_->register_item(metadata);
}

void metrics_store::register_aggregation(std::unique_ptr<metrics_aggregation> aggregation) {
    return body_->register_aggregation(std::move(aggregation));
}

bool metrics_store::unregister_element(std::string_view key) {
    return body_->unregister_element(key);
}

void metrics_store::enumerate_items(std::function<void(metrics_metadata const&, double)> const& acceptor) {
    return body_->enumerate_items(acceptor);
}

void metrics_store::enumerate_aggregations(std::function<void(metrics_aggregation const&)> const& acceptor) const {
    return body_->enumerate_aggregations(acceptor);
}

}
