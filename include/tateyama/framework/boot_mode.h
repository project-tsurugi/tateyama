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
#pragma once

#include <string_view>
#include <cstdint>
#include <cstdlib>
#include <ostream>

namespace tateyama::framework {

/**
 * @brief framework boot-up mode
 */
enum class boot_mode : std::uint32_t {
    unknown = 0,
    database_server,
    maintenance_server,
    maintenance_standalone,
    quiescent_server,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
[[nodiscard]] constexpr inline std::string_view to_string_view(boot_mode value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
        case boot_mode::unknown: return "unknown"sv;
        case boot_mode::database_server: return "database_server"sv;
        case boot_mode::maintenance_server: return "maintenance_server"sv;
        case boot_mode::maintenance_standalone: return "maintenance_standalone"sv;
        case boot_mode::quiescent_server: return "quiescent_server"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, boot_mode value) {
    return out << to_string_view(value);
}

}

