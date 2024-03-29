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

#include <tateyama/task_scheduler/context.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler {

/**
 * @brief basic task interface
 * @details this object is abstraction of task logic and state, and is used to submit task to scheduler.
 * Caller needs to inherit and implement the task content in the subclass.
 */
template<class...Impls>
class cache_align basic_task {
public:
    /**
     * @brief construct empty object
     */
    basic_task() = default;

    /**
     * @brief copy construct
     */
    basic_task(basic_task const&) = default;

    /**
     * @brief move construct
     */
    basic_task(basic_task &&) = default;

    /**
     * @brief copy assign
     */
    basic_task& operator=(basic_task const&) = default;

    /**
     * @brief move assign
     */
    basic_task& operator=(basic_task &&) = default;

    /**
     * @brief destruct task
     */
    ~basic_task() = default;

    /**
     * @brief move construct by moving base object
     */
    template <class T>
    explicit basic_task(T&& impl) : entity_(std::in_place_type<T>, std::forward<T>(impl)) {}

    /**
     * @brief execute the task
     * @param ctx the context information on the worker that is running the task
     */
    void operator()(context& ctx) {
        std::visit([&](auto&& arg){
            arg(ctx);
        }, entity_);
    }

    [[nodiscard]] bool sticky() {
        bool ret{};
        std::visit([&](auto&& arg){
            ret = arg.sticky();
        }, entity_);
        return ret;
    }

    //temporarily keep this public for testing // FIXME
    std::variant<Impls...> entity_{};  //NOLINT
};

}

