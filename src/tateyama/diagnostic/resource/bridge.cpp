/*
 * Copyright 2018-2023 tsurugi project.
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

#include "tateyama/diagnostic/resource/bridge.h"

namespace tateyama::diagnostic::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment&) {
    return true;
}

bool bridge::start(environment&) {
    return true;
}

bool bridge::shutdown(environment&) {
    return true;
}

void bridge::add_handler(const std::function<void(std::ostream&)>& func) {
    handlers_.emplace_back(func);
}

bridge::~bridge() = default;

void bridge::sighup_handler(int) {
    for (auto&& f : handlers_) {
        f(LOG(INFO));
    }
}

} // namespace tateyama::diagnostic::resource
