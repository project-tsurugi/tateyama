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

#include <cstdlib>

namespace tateyama::session::resource {

/**
 * @brief represents kind of shutdown request.
 */
enum class shutdown_request_type {
    /// @brief no shutdown request.
    nothing,
    /**
     * @brief safely shutdown the session.
     * @details
     * This operation terminates the session in the following order:
     * 
     * 1. reject subsequent new requests and respond `SESSION_CLOSED` to them.
     * 2. wait for "complete termination" of the current request, if any
     * 3. actually terminate the session
     * 
     * @note the stronger request type (e.g. shutdown_request_type::terminate) can override this request
     */
    graceful,
    /**
     * @brief terminates the session with cancelling ongoing requests.
     * @details
     * This operation terminates the session in the following order:
     * 
     * 1. reject subsequent new requests, and respond `SESSION_CLOSED` to them.
     * 2. tell cancellation to ongoing requests, and  respond `SESSION_CLOSED` to them.
     * 3. wait for "complete termination" of the current request, if any
     * 4. actually terminate the session
     */
    forceful,
};
/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(shutdown_request_type value) noexcept {
    using namespace std::string_view_literals;
    using kind = shutdown_request_type;
    switch (value) {
        case kind::nothing: return "nothing"sv;
        case kind::graceful: return "graceful"sv;
        case kind::forceful: return "forceful"sv;
    }
    std::abort();
}
/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, shutdown_request_type value) {
    return out << to_string_view(value);
}

}
