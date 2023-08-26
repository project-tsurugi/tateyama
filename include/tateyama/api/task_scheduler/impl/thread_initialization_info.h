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

namespace tateyama::task_scheduler {

class thread_control;

/**
 * @brief info gathered at the beginning of the thread
 */
class thread_initialization_info {
public:

    /**
     * @brief construct empty instance
     */
    thread_initialization_info() = default;

    /**
     * @brief destruct this instance
     */
    ~thread_initialization_info() = default;

    thread_initialization_info(thread_initialization_info const& other) = default;
    thread_initialization_info& operator=(thread_initialization_info const& other) = default;
    thread_initialization_info(thread_initialization_info&& other) noexcept = default;
    thread_initialization_info& operator=(thread_initialization_info&& other) noexcept = default;

    /**
     * @brief construct new instance
     */
    explicit thread_initialization_info(
        std::size_t thread_id,
        thread_control* thread = nullptr
    ) noexcept :
        thread_id_(thread_id),
        thread_(thread)
    {}

    /**
     * @brief accessor to the thread id
     * @return thread id
     */
    [[nodiscard]] std::size_t thread_id() const noexcept {
        return thread_id_;
    }

    /**
     * @brief accessor to the thread control
     * @return thread control
     */
    [[nodiscard]] thread_control* thread() const noexcept {
        return thread_;
    }

private:
    std::size_t thread_id_{};
    thread_control* thread_{};
};

}
