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

#include "tateyama/session/resource/bridge.h"

namespace tateyama::session::resource {

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

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

std::string_view bridge::label() const noexcept {
    return component_label;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::list() {
    return std::nullopt;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::get([[maybe_unused]] std::string_view session_specifier) {
    return std::nullopt;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::shutdown([[maybe_unused]] std::string_view session_specifier, [[maybe_unused]] shutdown_request_type type) {
    return std::nullopt;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::set_valiable([[maybe_unused]] std::string_view session_specifier, [[maybe_unused]] std::string_view name, [[maybe_unused]] std::string_view value) {
    return std::nullopt;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::get_valiable([[maybe_unused]] std::string_view session_specifier, [[maybe_unused]] std::string_view name) {
    return std::nullopt;
}

}
