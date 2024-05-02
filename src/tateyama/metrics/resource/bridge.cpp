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

#include <memory>

#include "tateyama/metrics/resource/bridge.h"

namespace tateyama::metrics::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment&) {
    auto metrics_store = std::make_unique<metrics_store_impl>();
    core_.metrics_store_impl_ = metrics_store.get();
    metrics_store_ = std::make_unique<tateyama::metrics::metrics_store>(std::move(metrics_store));
    return true;
}

bool bridge::start(environment&) {
    return true;
}

bool bridge::shutdown(environment&) {
    return true;
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

std::string_view bridge::label() const noexcept {
    return component_label;
}

tateyama::metrics::resource::core const& bridge::core() const noexcept {
    return core_;
}

}
