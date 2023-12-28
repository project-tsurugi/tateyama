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

#include <atomic>

#include <takatori/util/assertion.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler::impl {

/**
 * @brief util class to get the notification periodically while clients count up
 * @note this object is thread-unsafe and should be used by a single thread
 */
class cache_align periodic_notifier {
public:

    /**
     * @brief construct default instance
     */
    periodic_notifier() = default;

    /**
     * @brief construct instance
     * @param notifications the numerator of notification ratio
     * @param period the denominator of the notification ratio
     * @details client gets the notifications in accordance with the given notification ratio.
     * The number of notifications that equals to `notifications` will be made by the time `period` count of
     * `count_up` calls finish.
     */
    periodic_notifier(
        std::size_t notifications,
        std::size_t period
    ) noexcept {
        init(notifications, period);
    }

    /**
     * @brief count up and get notified if notification condition is met
     * @return true if notification condition is met
     * @return false otherwise
     */
    bool count_up() {
        current_ += notifications_;
        if(current_ >= period_) {
            current_ -= period_;
            return true;
        }
        return false;
    }

    /**
     * @brief (re-)init the instance
     * @param notifications the numerator of notification ratio
     * @param period the denominator of the notification ratio
     */
    void init(
        std::size_t notifications,
        std::size_t period
    ) {
        BOOST_ASSERT(notifications != 0);  //NOLINT
        BOOST_ASSERT(period != 0);  //NOLINT
        BOOST_ASSERT(notifications <= period);  //NOLINT
        notifications_ = notifications;
        period_ = period;
        current_ = 0;
    }

    void reset() {
        current_ = 0;
    }

private:
    std::size_t notifications_{1};
    std::size_t period_{1};
    std::size_t current_{};
};

}  // namespace tateyama::task_scheduler::impl
