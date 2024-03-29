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

#include <tbb/concurrent_queue.h>
#include <glog/logging.h>

#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler::impl {

template <class T>
class cache_align tbb_queue {
public:
    using task = T;

    /**
     * @brief construct empty instance
     */
    tbb_queue() = default;

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
        return origin_.unsafe_size();
    }

    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    void clear() {
        origin_.clear();
    }

    void reconstruct() {
        origin_.~concurrent_queue();
        new (&origin_)tbb::concurrent_queue<task>();
    }
private:
    tbb::concurrent_queue<task> origin_{};

};

}
