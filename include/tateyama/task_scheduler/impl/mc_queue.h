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

#include <cstdint>
#include <variant>
#include <ios>
#include <functional>

#include <glog/logging.h>

#include <concurrentqueue/concurrentqueue.h>

#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler::impl {

template <class T>
class cache_align mc_queue {
public:
    using task = T;
    /**
     * @brief construct empty instance
     */
    mc_queue() = default;

    void push(task const& t) {
        origin_.enqueue(t);
    }

    void push(task&& t) {
        origin_.enqueue(std::move(t));
    }

    bool try_pop(task& t) {
        return origin_.try_dequeue(t);
    }

    [[nodiscard]] std::size_t size() const {
        return origin_.size_approx();
    }

    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    void clear() {
        task value;
        while( !empty() ) {
            try_pop(value);
        }
    }
    void reconstruct() {
        origin_.~ConcurrentQueue();
        new (&origin_)moodycamel::ConcurrentQueue<task>();
    }
private:
    moodycamel::ConcurrentQueue<task> origin_{};

};

}
