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

#include "tateyama/diagnostic/resource/diagnostic_resource.h"

namespace tateyama::diagnostic::resource {

using namespace framework;

component::id_type diagnostic_resource::id() const noexcept {
    return tag;
}

bool diagnostic_resource::setup(environment&) {
    return true;
}

bool diagnostic_resource::start(environment&) {
    return true;
}

bool diagnostic_resource::shutdown(environment&) {
    return true;
}

diagnostic_resource::diagnostic_resource() {
}

diagnostic_resource::~diagnostic_resource() = default;

void diagnostic_resource::add_print_callback(std::string_view id, const std::function<void(std::ostream&)>& func) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool found = false;
    for (auto&& e : handlers_) {
        if (e.first == id) {
            found = true;
            break;
        }
    }
    if (!found) {
        handlers_.emplace_back(std::string(id), func);
    }
}

void diagnostic_resource::diagnostic_resource::remove_print_callback(std::string_view id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto r = std::remove_if(handlers_.begin(), handlers_.end(), [id](const std::pair<std::string, std::function<void(std::ostream&)>>& e) { return e.first == id; });
    handlers_.erase(r, handlers_.end());
}

void diagnostic_resource::print_diagnostics(std::ostream& out) {
    for (auto& e : handlers_) {
        e.second(out);
    }
}

} // namespace tateyama::diagnostic::resource
