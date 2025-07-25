/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <string_view>

namespace tateyama::endpoint::common {

using namespace std::string_view_literals;

static constexpr std::string_view ipc_endpoint_config_prefix = "/:tateyama:ipc_endpoint:config: "sv;

static constexpr std::string_view stream_endpoint_config_prefix = "/:tateyama:stream_endpoint:config: "sv;

static constexpr std::string_view session_config_prefix = "/:tateyama:session:config: "sv;

static constexpr std::string_view authentication_config_prefix = "/:tateyama:authentication:config: "sv;

} // namespace
