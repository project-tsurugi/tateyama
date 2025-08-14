/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <mutex>
#include <chrono>

#include <tateyama/session/context.h>

namespace tateyama::endpoint::common {

class alive_timer {
public:
    explicit alive_timer(std::size_t timeout) :
        timeout_(timeout), enable_timeout_(timeout > 0) {
        if (enable_timeout_) {
            update_expiration_time(true);
        }
    }

    void update_expiration_time(bool force = false) {
        if (enable_timeout_) {
            auto new_until_time = tateyama::session::session_context::expiration_time_type::clock::now() + timeout_;
            if (force) {
                expiration_time_opt_ = new_until_time;
            } else if (expiration_time_opt_) {
                // Only update if the new expiration time is further in the future than the current one,
                // i.e., we only extend the timeout, never shorten it.
                if (expiration_time_opt_.value() < new_until_time) {
                    expiration_time_opt_ = new_until_time;
                }
            }
        }
    }

    bool is_expiration_time_over() {
        if (expiration_time_opt_) {
            return tateyama::session::session_context::expiration_time_type::clock::now() > expiration_time_opt_.value();
        }
        return false;
    }

    [[nodiscard]] std::uint64_t next_expiration() const noexcept {
        if (enable_timeout_ && expiration_time_opt_) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(expiration_time_opt_.value().time_since_epoch()).count();
        }
        return 0;
    }

private:
    std::chrono::seconds timeout_;
    bool enable_timeout_;
    std::optional<tateyama::session::session_context::expiration_time_type> expiration_time_opt_{};
};

}  // tateyama::endpoint::common
