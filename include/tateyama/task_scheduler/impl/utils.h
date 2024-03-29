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

#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>

#include <takatori/util/detect.h>
#include "thread_initialization_info.h"

namespace tateyama::task_scheduler::impl {

namespace details {

template<class T, class ...Args>
using with_init_type =
    decltype(std::declval<T&>().init(static_cast<thread_initialization_info const&>(thread_initialization_info{}), std::declval<Args>()...));

}

/**
 * @brief utility to check if given type has the init(thread_initialization_info, args...) member function
 * @tparam T
 */
template<class T, class ...Args>
inline constexpr bool has_init_v = takatori::util::is_detected_v<details::with_init_type, T, Args...>;

}

