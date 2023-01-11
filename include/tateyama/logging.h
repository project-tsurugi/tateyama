/*
 * Copyright 2018-2021 tsurugi project.
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

#include <cstdint>
#include <array>

namespace tateyama {

/**
 * @brief logging level constant for errors
 */
static constexpr std::int32_t log_error = 10;

/**
 * @brief logging level constant for warnings
 */
static constexpr std::int32_t log_warning = 20;

/**
 * @brief logging level constant for information
 */
static constexpr std::int32_t log_info = 30;

/**
 * @brief logging level constant for debug information
 */
static constexpr std::int32_t log_debug = 40;

/**
 * @brief logging level constant for traces
 */
static constexpr std::int32_t log_trace = 50;


constexpr std::string_view find_fullname(std::string_view prettyname, std::string_view funcname) {
    // search (funcname + "(")
    size_t fn_pos = 0;  // head of function name
    while ((fn_pos = prettyname.find(funcname, fn_pos)) != std::string_view::npos) {
        if (prettyname[fn_pos + funcname.size()] == '(') {
            break;  // found
        }
        fn_pos++;
    }
    if (fn_pos == std::string_view::npos) {
        return funcname;  // fallback
    }
    // search to left, but skip <...>
    size_t start_pos = std::string_view::npos;
    int tv_nest = 0;  // "<...>" nest level
    for (int i = fn_pos; i >= 0; i--) {
        switch (prettyname[i]) {
            case '>': tv_nest++; continue;
            case '<': tv_nest--; continue;
            case ' ': if (tv_nest <= 0) start_pos = i+1; break;
        }
        if (start_pos != std::string_view::npos) {
            break;
        }
    }
    if (start_pos == std::string_view::npos) {  // no return type, such as constructors
        start_pos = 0;
    }
    return std::string_view(prettyname.data() + start_pos, fn_pos + funcname.length() - start_pos);
}

template<size_t N>
constexpr auto location_prefix(const std::string_view sv) {
    std::array<char, N + 3> buf{};

    // tsurugi logging location prefix:
    // * "::" -> ":"
    // * "<...>" -> ""
    // * [-A-Za-z0-9_:] only
    int p = 0;
    buf.at(p++) = '/';
    buf.at(p++) = ':';
    int tv_nest = 0;  // "<...>" nest level
    for (size_t i = 0; i < sv.size(); i++) {
        if (sv[i] == '<') {
            tv_nest++;
        } else if (sv[i] == '>') {
            tv_nest--;
        } else {
            if (tv_nest <= 0) {
                if (sv[i] == ':' && sv[i + 1] == ':') {
                    buf.at(p++) = ':';
                    i++;  // skip second colon
                } else {
                    buf.at(p++) = sv[i];
                }
            }
        }
    }
    buf.at(p++) = ' ';
    buf.at(p) = 0;
    return buf;
}

template<size_t N, size_t M>
constexpr auto location_prefix(const char (&prettyname)[N], const char (&funcname)[M]) {  // NOLINT
    const std::string_view sv = find_fullname(prettyname, funcname);  // NOLINT
    return location_prefix<std::max(N, M)>(sv);
}

// NOLINTNEXTLINE
#define _LOCATION_PREFIX_ location_prefix(__PRETTY_FUNCTION__, __FUNCTION__).data()
// NOLINTNEXTLINE
#define LOG_LP(x)   LOG(x)   << _LOCATION_PREFIX_
// NOLINTNEXTLINE
#define VLOG_LP(x)  VLOG(x)  << _LOCATION_PREFIX_
// NOLINTNEXTLINE
#define DVLOG_LP(x) DVLOG(x) << _LOCATION_PREFIX_

} // namespace
