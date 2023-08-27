/*
 * Copyright 2018-2021 tsurugi project.
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

#include <tateyama/task_scheduler/context.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler {

/**
 * @brief basic conditional task interface
 * @details this object is abstraction of conditional task logic and state, and is used to submit task to scheduler.
 */
class cache_align basic_conditional_task {
public:

    /**
     * @brief condition type
     */
    using condition_type = std::function<bool(void)>;

    /**
     * @brief body type
     */
    using body_type = std::function<void(void)>;

    /**
     * @brief construct empty object
     */
    basic_conditional_task() = default;

    /**
     * @brief copy construct
     */
    basic_conditional_task(basic_conditional_task const&) = default;

    /**
     * @brief move construct
     */
    basic_conditional_task(basic_conditional_task &&) = default;

    /**
     * @brief copy assign
     */
    basic_conditional_task& operator=(basic_conditional_task const&) = default;

    /**
     * @brief move assign
     */
    basic_conditional_task& operator=(basic_conditional_task &&) = default;

    /**
     * @brief destruct task
     */
    ~basic_conditional_task() = default;

    /**
     * @brief construct new object
     */
    basic_conditional_task(
        condition_type condition,
        body_type body
    ) noexcept :
        condition_(std::move(condition)),
        body_(std::move(body))
    {}

    /**
     * @brief check the condition for the task to be ready
     */
    bool check() {
        return condition_();
    }

    /**
     * @brief execute the task body
     */
    void operator()() {
        body_();
    }

private:
    condition_type condition_{};
    body_type body_{};

};

}

