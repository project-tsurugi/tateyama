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
#include <deque>

#include <glog/logging.h>
#include <boost/rational.hpp>
#include <boost/exception/all.hpp>
#include <takatori/util/exception.h>

#include <tateyama/common.h>
#include <tateyama/api/task_scheduler/context.h>
#include <tateyama/api/task_scheduler/impl/queue.h>
#include <tateyama/api/task_scheduler/impl/thread_control.h>
#include <tateyama/api/task_scheduler/task_scheduler_cfg.h>
#include <tateyama/api/task_scheduler/impl/utils.h>
#include <tateyama/api/task_scheduler/impl/backoff_waiter.h>

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

    /**
     * @brief create empty object
     */
    worker() = default;

    /**
     * @brief create new object
     * @param queues reference to the queues
     * @param sticky_task_queues reference to the sticky task queues
     * @param delayed_task_queues reference to the delayed task queues
     * @param initial_tasks reference initial tasks (ones submitted before starting scheduler)
     * @param stat worker stat information
     * @param cfg the scheduler configuration information
     */
    worker(
        std::vector<basic_queue<task>>& queues,
        std::vector<basic_queue<task>>& sticky_task_queues,
        std::vector<basic_queue<task>>& delayed_task_queues,
        std::vector<std::vector<task>>& initial_tasks,
        worker_stat& stat,
        task_scheduler_cfg const* cfg = nullptr
    ) noexcept:
        cfg_(cfg),
        queues_(std::addressof(queues)),
        sticky_task_queues_(std::addressof(sticky_task_queues)),
        delayed_task_queues_(std::addressof(delayed_task_queues)),
        initial_tasks_(std::addressof(initial_tasks)),
        stat_(std::addressof(stat)),
        waiter_(cfg->lazy_worker() ? backoff_waiter() : backoff_waiter(0))
    {}

    /**
     * @brief initialize the worker
     * @param thread_id the thread index assigned for this worker
     */
    void init(std::size_t thread_id) {
        // reconstruct the queues so that they are on each numa node
        auto index = thread_id;
        (*queues_)[index].reconstruct();
        (*sticky_task_queues_)[index].reconstruct();
        auto& q = (*queues_)[index];
        auto& sq = (*sticky_task_queues_)[index];
        auto& dtq = (*delayed_task_queues_)[index];
        auto& s = (*initial_tasks_)[index];
        for(auto&& t : s) {
            if(t.delayed()) {
                // t can be sticky && delayed
                dtq.push(std::move(t));
                continue;
            }
            if(t.sticky()) {
                sq.push(std::move(t));
                continue;
            }
            q.push(std::move(t));
        }
        s.clear();
    }

    /**
     * @brief proceed one step
     * @param ctx the scheduler context
     * @param q the local task queue for this worker
     * @param sq the sticky task queue for this worker
     * @note this function is kept public just for testing
     */
    bool process_next(
        api::task_scheduler::context& ctx,
        basic_queue<task>& q,
        basic_queue<task>& sq
    ) {
        if (try_local_and_sticky(ctx, q, sq)) {
            return true;
        }
        if (cfg_ && cfg_->stealing_enabled()) {
            // try local and sticky more times before stealing
            for(std::size_t i=0, n=cfg_->stealing_wait() * cfg_->thread_count(); i<n ; ++i) {
                if(try_local_and_sticky(ctx, q, sq)) {
                    return true;
                }
            }
            bool stolen = steal_and_execute(ctx);
            if(stolen) {
                return true;
            }
        }
        if(cfg_ && cfg_->lazy_worker() && check_delayed_task_exists(ctx)) {
            // If delayed task exists, pretend as if its small slice is executed so that worker won't suspend.
            return true;
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
                waiter_();
            } else {
                waiter_.reset();
            }
        }
    }

private:
    task_scheduler_cfg const* cfg_{};
    std::vector<basic_queue<task>>* queues_{};
    std::vector<basic_queue<task>>* sticky_task_queues_{};
    std::vector<basic_queue<task>>* delayed_task_queues_{};
    std::vector<std::vector<task>>* initial_tasks_{};
    worker_stat* stat_{};
    backoff_waiter waiter_{0};

    std::size_t next(std::size_t current) {
        auto sz = queues_->size();
        if (current == sz - 1) {
            return 0;
        }
        return current + 1;
    }

    bool steal_and_execute(api::task_scheduler::context& ctx) {
        std::size_t last = ctx.last_steal_from();
        task t{};
        for(auto idx = next(last); idx != last; idx = next(idx)) {
            auto& tgt = (*queues_)[idx];
            if(tgt.active() && tgt.try_pop(t)) {
                ++stat_->stolen_;
                ctx.last_steal_from(idx);
                ctx.task_is_stolen(true);
                execute_task(t, ctx);
                ctx.task_is_stolen(false);
                ++stat_->count_;
                return true;
            }
        }
        return false;
    }

    void execute_task(task& t, api::task_scheduler::context& ctx) {
        try {
            // use try-catch to avoid server crash even on fatal internal error
            t(ctx);
        } catch (boost::exception& e) {
            // currently find_trace() after catching std::exception doesn't work properly. So catch as boost exception. TODO
            LOG(ERROR) << "Unhandled boost exception caught.";
            LOG(ERROR) << boost::diagnostic_information(e);
        } catch (std::exception& e) {
            LOG(ERROR) << "Unhandled exception caught: " << e.what();
            if(auto* tr = takatori::util::find_trace(e); tr != nullptr) {
                LOG(ERROR) << *tr;
            }
        }
    }

    bool try_process(
        api::task_scheduler::context& ctx,
        basic_queue<task>& q
    ) {
        task t{};
        if (q.active() && q.try_pop(t)) {
            execute_task(t, ctx);
            ++stat_->count_;
            return true;
        }
        return false;
    }

    bool try_local_and_sticky(
        api::task_scheduler::context& ctx,
        basic_queue<task>& q,
        basic_queue<task>& sq
    ) {
        promote_delayed_task_if_needed(ctx, q, sq);
        // using counter, check sticky sometimes for fairness
        auto& cnt = ctx.count_check_local_first();
        cnt += cfg_->ratio_check_local_first();
        if(cnt < 1) {
            if(try_process(ctx, sq)) return true;
            if(try_process(ctx, q)) return true;
        } else {
            --cnt;
            if(try_process(ctx, q)) return true;
            if(try_process(ctx, sq)) return true;
        }
        return false;
    }


    void promote_delayed_task_if_needed(
        api::task_scheduler::context& ctx,
        basic_queue<task>& q,
        basic_queue<task>& sq
    ) {
        auto& cnt = ctx.count_promoting_delayed();
        cnt += cfg_->frequency_promoting_delayed();
        if(cnt >= 1) {
            --cnt;
            auto& dtq = (*delayed_task_queues_)[ctx.index()];
            task dt{};
            if(dtq.try_pop(dt)) {
                if(dt.sticky()) {
                    sq.push(std::move(dt));
                } else {
                    q.push(std::move(dt));
                }
            }
        }
    }

    bool check_delayed_task_exists(
        api::task_scheduler::context& ctx
    ) {
        // caution: this logic re-order an element (if exists) in the queue, and can result in live-lock
        // e.g. some task always get re-ordered and never executed. Use with care.
        auto& dtq = (*delayed_task_queues_)[ctx.index()];
        task dt{};
        if(dtq.try_pop(dt)) {
            dtq.push(std::move(dt));
            return true;
        }
        return false;
    }

};

}
