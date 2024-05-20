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

#include "resource/metrics_item_slot_impl.h"

namespace tateyama::metrics {

metrics_item_slot::metrics_item_slot(std::shared_ptr<tateyama::metrics::resource::metrics_item_slot_impl> arg) noexcept : body_(std::move(arg)) {
}

void metrics_item_slot::set(double value) {
    body_->set(value);
}

metrics_item_slot& metrics_item_slot::operator=(double value) {
    body_->set(value);
    return *this;
}

}
