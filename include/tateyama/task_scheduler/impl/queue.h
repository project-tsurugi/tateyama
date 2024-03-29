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

#include "tbb_queue.h"
#ifdef MC_QUEUE
#include "mc_queue.h"
#endif
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler::impl {

template <class T>
class cache_align basic_queue {

public:
    using task = T;

#ifdef MC_QUEUE
    using queue_type = mc_queue<task>;
#else
  #ifdef STD_QUEUE
    using queue_type = std_queue<task>;
  #else
    using queue_type = tbb_queue<task>;
  #endif
#endif

    /**
     * @brief construct empty instance
     */
    basic_queue() = default;

    void push(task const& t) {
        origin_.push(t);
    }

    void push(task&& t) {
        origin_.push(std::move(t));
    }

    bool try_pop(task& t) {
        return origin_.try_pop(t);
    }

    [[nodiscard]] std::size_t size() const {
        return origin_.size();
    }

    [[nodiscard]] bool empty() const {
        return origin_.empty();
    }

    void clear() {
        origin_.clear();
    }

    void reconstruct() {
        origin_.reconstruct();
    }

    /**
     * @brief deactivate the queue
     * @details this can be used to notify worker threads of finishing the job
     */
    void deactivate() {
        return active_->store(false);
    }

    /**
     * @brief active flag
     * @details worker threads should check this before dequeue element and exit if false
     */
    bool active() {
        return active_->load();
    }
private:
    // use unique_ptr for movability
    std::unique_ptr<std::atomic_bool> active_{std::make_unique<std::atomic_bool>(true)};
    queue_type origin_{};
};

}
