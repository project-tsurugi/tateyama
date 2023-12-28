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
#include <tateyama/utils/cache_align.h>
#include <tateyama/task_scheduler/impl/periodic_notifier.h>

namespace tateyama::task_scheduler::impl {
class thread_control;
}

namespace tateyama::task_scheduler {

/**
 * @brief worker context information
 * @details dynamic context information of the worker
 */
class cache_align context {
public:
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
     * @brief accessor to the local_first_notifer parameter
     * @return notifier used to get notification when to check local task queue first
     */
    [[nodiscard]] impl::periodic_notifier& local_first_notifer() noexcept {
        return local_first_notifier_;
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
    [[nodiscard]] impl::thread_control* thread() const noexcept {
        return thread_;
    }

    /**
     * @brief setter to the thread that runs the worker with this context
     * @param arg the thread
     */
    void thread(impl::thread_control* arg) noexcept {
        thread_ = arg;
    }

    /**
     * @brief accessor to the busy_working flag
     * @return whether the worker has been working busily or suspended
     */
    [[nodiscard]] bool busy_working() const noexcept {
        return busy_working_;
    }

    /**
     * @brief setter to the busy_working flag
     * @param arg whether the worker has been working busily or suspended
     */
    void busy_working(bool arg) noexcept {
        busy_working_ = arg;
    }

private:
    std::size_t index_{};
    std::size_t last_steal_from_{};
    impl::periodic_notifier local_first_notifier_{};
    bool task_is_stolen_{};
    impl::thread_control* thread_{};
    bool busy_working_{};
};

}
