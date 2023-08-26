/*
 * Copyright 2018-2020 tsurugi project.
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
#include <boost/rational.hpp>

#include <glog/logging.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler {
class thread_control;
}

namespace tateyama::api::task_scheduler {

/**
 * @brief worker context information
 * @details dynamic context information of the worker
 */
class cache_align context {
public:
    using rational = boost::rational<std::int64_t>;

    context() = default;
    ~context() = default;
    context(context const& other) = delete;
    context& operator=(context const& other) = delete;
    context(context&& other) noexcept = default;
    context& operator=(context&& other) noexcept = default;

    /**
     * @brief create new context object
     * @param index the worker index
     */
    explicit context(std::size_t index) noexcept :
        index_(index)
    {}

    /**
     * @brief accessor to the worker index
     * @return the 0-origin index of the worker, that is associated with this context
     */
    [[nodiscard]] std::size_t index() const noexcept {
        return index_;
    }

    /**
     * @brief accessor to the last steal worker index
     * @return the 0-origin index of the worker, where the worker stole tasks from.
     */
    [[nodiscard]] std::size_t last_steal_from() const noexcept {
        return last_steal_from_;
    }

    /**
     * @brief setter to the last steal worker index
     * @param the 0-origin index of the worker, where the worker stole tasks from.
     */
    void last_steal_from(std::size_t arg) noexcept {
        last_steal_from_ = arg;
    }

    /**
     * @brief accessor to the count_check_local_first parameter
     * @return counter used to accumulate ratio_check_local_first
     */
    [[nodiscard]] rational& count_check_local_first() noexcept {
        return count_check_local_first_;
    }

    /**
     * @brief accessor to the count_promoting_sleep parameter
     * @return counter used to accumulate frequentcy_promoting_delayed
     */
    [[nodiscard]] rational& count_promoting_delayed() noexcept {
        return count_promoting_delayed_;
    }

    /**
     * @brief accessor to the stealing flag
     * @return whether the task is being executed by stealing
     */
    [[nodiscard]] bool task_is_stolen() const noexcept {
        return task_is_stolen_;
    }

    /**
     * @brief setter to the stealing flag
     * @param whether the task is being executed by stealing
     */
    void task_is_stolen(bool arg) noexcept {
        task_is_stolen_ = arg;
    }

    /**
     * @brief accessor to the thread that runs the worker with this context
     * @return the thread
     */
    [[nodiscard]] tateyama::task_scheduler::thread_control* thread() const noexcept {
        return thread_;
    }

    /**
     * @brief setter to the thread that runs the worker with this context
     * @param arg the thread
     */
    void thread(tateyama::task_scheduler::thread_control* arg) noexcept {
        thread_ = arg;
    }
private:
    std::size_t index_{};
    std::size_t last_steal_from_{};
    rational count_check_local_first_{};
    rational count_promoting_delayed_{};
    bool task_is_stolen_{};
    tateyama::task_scheduler::thread_control* thread_{};
};

}
