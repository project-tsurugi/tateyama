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
#include <iostream>

#include "stream.h"
#include "stream_response.h"

namespace tateyama::endpoint::stream {

std::atomic_uint64_t stream_socket::num_open_{0};         // NOLINT
std::mutex stream_socket::num_mutex_{};                   // NOLINT
std::condition_variable stream_socket::num_condition_{};  // NOLINT

};
