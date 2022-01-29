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
#include <emmintrin.h>

#include <glog/logging.h>

#include <tateyama/common.h>
#include <tateyama/api/task_scheduler/context.h>
#include <tateyama/api/task_scheduler/impl/queue.h>
#include <tateyama/api/task_scheduler/impl/thread_control.h>
#include <tateyama/api/task_scheduler/task_scheduler_cfg.h>
#include <tateyama/api/task_scheduler/impl/utils.h>

namespace tateyama::task_scheduler {

using api::task_scheduler::task_scheduler_cfg;

struct cache_align worker_stat {
    std::size_t count_{};
    std::size_t stolen_{};
};

/**
 * @brief worker object
 * @details this represents the worker logic running on each thread that processes its local queue
 * @note this object is just a logic object and doesn't hold dynamic state, so safely be copied into thread_control.
 */
template <class T>
class cache_align worker {
public:
    using task = T;

    using initialize_function_type = std::function<void(std::size_t)>;

    /**
     * @brief create empty object
     */
    worker() = default;

    /**
     * @brief create new object
     * @param queues reference to the queues
     * @param stat worker stat information
     * @param cfg the scheduler configuration information
     */
    worker(
        std::vector<basic_queue<task>>& queues,
        std::vector<basic_queue<task>>& sticky_task_queues,
        std::vector<std::vector<task>>& initial_tasks,
        worker_stat& stat,
        initialize_function_type init_fn,
        task_scheduler_cfg const* cfg = nullptr
    ) noexcept:
        cfg_(cfg),
        queues_(std::addressof(queues)),
        sticky_task_queues_(std::addressof(sticky_task_queues)),
        initial_tasks_(std::addressof(initial_tasks)),
        stat_(std::addressof(stat)),
        init_fn_(std::move(init_fn))
    {}

    /**
     * @brief the worker body
     * @param ctx the worker context information
     */
    void init(std::size_t thread_id) {
        // reconstruct the queues so that they are on each numa node
        auto index = thread_id;
        (*queues_)[index].reconstruct();
        (*sticky_task_queues_)[index].reconstruct();
        auto& q = (*queues_)[index];
        auto& sq = (*sticky_task_queues_)[index];
        auto& s = (*initial_tasks_)[index];
        for(auto&& t : s) {
            if(t.sticky()) {
                sq.push(std::move(t));
                continue;
            }
            q.push(std::move(t));
        }
        s.clear();

        if(init_fn_) {
            init_fn_(thread_id);
        }
    }

    bool process_next(
        api::task_scheduler::context& ctx,
        basic_queue<task>& q,
        basic_queue<task>& sq
    ) {
        task t{};
        if (sq.active() && sq.try_pop(t)) {
            t(ctx);
            ++stat_->count_;
            return true;
        }
        if (q.active() && q.try_pop(t)) {
            t(ctx);
            ++stat_->count_;
            return true;
        }
        if (cfg_ && cfg_->stealing_enabled()) {
            bool stolen = steal_and_execute(ctx);
            if(stolen) {
                return true;
            }
        }
        return false;
    }
    /**
     * @brief the worker body
     * @param ctx the worker context information
     */
    void operator()(api::task_scheduler::context& ctx) {
        auto index = ctx.index();
        auto& q = (*queues_)[index];
        auto& sq = (*sticky_task_queues_)[index];
        ctx.last_steal_from(index);
        while(sq.active() || q.active()) {
            if(! process_next(ctx, q, sq)) {
                _mm_pause();
            }
        }
    }

private:
    task_scheduler_cfg const* cfg_{};
    std::vector<basic_queue<task>>* queues_{};
    std::vector<basic_queue<task>>* sticky_task_queues_{};
    std::vector<std::vector<task>>* initial_tasks_{};
    worker_stat* stat_{};
    initialize_function_type init_fn_{};

    std::size_t next(std::size_t current, std::size_t initial) {
        (void)initial;
        auto sz = queues_->size();
        if (current == sz - 1) {
            return 0;
        }
        return current + 1;
    }

    bool steal_and_execute(api::task_scheduler::context& ctx) {
        auto index = ctx.index();
        std::size_t from = ctx.last_steal_from();
        task t{};
        for(auto idx = next(from, from); idx != from; idx = next(idx, from)) {
            auto& tgt = (*queues_)[idx];
            if(tgt.try_pop(t)) {
                ++stat_->stolen_;
                ctx.last_steal_from(idx);
                DLOG(INFO) << "task stolen from queue " << idx << " to " << index;
                t(ctx);
                ++stat_->count_;
                return true;
            }
        }
        return false;
    }

};

}
