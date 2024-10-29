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
#include <emmintrin.h>
#include <deque>
#include <thread>

#include <glog/logging.h>
#include <boost/rational.hpp>
#include <boost/exception/all.hpp>
#include <takatori/util/exception.h>

#include <tateyama/common.h>
#include <tateyama/task_scheduler/context.h>
#include <tateyama/task_scheduler/impl/queue.h>
#include <tateyama/task_scheduler/impl/thread_control.h>
#include <tateyama/task_scheduler/impl/thread_initialization_info.h>
#include <tateyama/task_scheduler/task_scheduler_cfg.h>
#include <tateyama/task_scheduler/impl/utils.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler::impl {

struct cache_align worker_stat {
    /**
     * @brief total number of tasks (including both normal/sticky, and stolen tasks) executed by the worker
     */
    std::size_t count_{};

    /**
     * @brief the number of tasks stolen (from other workers) and executed by the worker
     */
    std::size_t steal_{};

    /**
     * @brief the number of sticky tasks executed by the worker
     */
    std::size_t sticky_{};

    /**
     * @brief the count of when worker gets up (from suspension) and executes at least one task
     */
    std::size_t wakeup_run_{};

    /**
     * @brief the count of worker suspends
     */
    std::size_t suspend_{};
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

    using initializer_type = std::function<void(std::size_t)>;

    /**
     * @brief create empty object
     */
    worker() = default;

    /**
     * @brief create new object
     * @param queues reference to the queues
     * @param sticky_task_queues reference to the sticky task queues
     * @param initial_tasks reference initial tasks (ones submitted before starting scheduler)
     * @param stat worker stat information
     * @param cfg the scheduler configuration information
     * @param initializer the function called on worker thread for initialization
     */
    worker(
        std::vector<basic_queue<task>>& queues,
        std::vector<basic_queue<task>>& sticky_task_queues,
        std::vector<std::vector<task>>& initial_tasks,
        worker_stat& stat,
        task_scheduler_cfg const& cfg,
        initializer_type initializer = {}
    ) noexcept:
        cfg_(std::addressof(cfg)),
        queues_(std::addressof(queues)),
        sticky_task_queues_(std::addressof(sticky_task_queues)),
        initial_tasks_(std::addressof(initial_tasks)),
        stat_(std::addressof(stat)),
        initializer_(std::move(initializer))
    {}

    /**
     * @brief initialize the worker
     * @param thread_id the thread index assigned for this worker
     */
    void init(thread_initialization_info const& info, context& ctx) {
        // reconstruct the queues so that they are on each numa node
        ctx.thread(info.thread());
        auto index = info.thread_id();
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

        if(initializer_) {
            initializer_(index);
        }
    }

    /**
     * @brief proceed one step
     * @param ctx the scheduler context
     * @param q the local task queue for this worker
     * @param sq the sticky task queue for this worker
     * @note this function is kept public just for testing
     */
    bool process_next(
        context& ctx,
        basic_queue<task>& q,
        basic_queue<task>& sq
    ) {
        if (try_local_and_sticky(ctx, q, sq)) {
            return true;
        }
        if (cfg_->stealing_enabled()) {
            // try local and sticky more times before stealing
            for(std::size_t i=0, n=cfg_->stealing_wait() * cfg_->thread_count(); i<n ; ++i) {
                if(try_local_and_sticky(ctx, q, sq)) {
                    return true;
                }
                _mm_pause();
            }
            bool stolen = steal_and_execute(ctx);
            if(stolen) {
                ++stat_->steal_;
                return true;
            }
        }
        auto pw = cfg_->task_polling_wait();
        if(pw > 0) { // if 0, no wait
            if(pw == 1) {
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds{pw-2});
            }
        }
        return false;
    }

    void suspend_worker_if_needed(
        std::size_t& empty_work_count,
        context& ctx
    ) {
        if(! cfg_->busy_worker()) {
            ++empty_work_count;
            if(empty_work_count > cfg_->worker_try_count()) {
                empty_work_count = 0;
                ctx.busy_working(false);
                ++stat_->suspend_;
                ctx.thread()->suspend(std::chrono::microseconds{cfg_->worker_suspend_timeout()});
            }
        }
    }
    /**
     * @brief the worker body
     * @param ctx the worker context information
     */
    void operator()(context& ctx) {
        auto index = ctx.index();
        auto& q = (*queues_)[index];
        auto& sq = (*sticky_task_queues_)[index];
        ctx.last_steal_from(index);
        std::size_t empty_work_count = 0;
        while(sq.active() || q.active()) {
            if(! process_next(ctx, q, sq)) {
                _mm_pause();
                if(! sq.active() && ! q.active()) break;
                suspend_worker_if_needed(empty_work_count, ctx);
            } else {
                empty_work_count = 0;
            }
        }
    }

private:
    task_scheduler_cfg const* cfg_{};
    std::vector<basic_queue<task>>* queues_{};
    std::vector<basic_queue<task>>* sticky_task_queues_{};
    std::vector<std::vector<task>>* initial_tasks_{};
    worker_stat* stat_{};
    initializer_type initializer_{};

    std::size_t next(std::size_t current) {
        auto sz = queues_->size();
        if (current == sz - 1) {
            return 0;
        }
        return current + 1;
    }

    bool steal_and_execute(context& ctx) {
        std::size_t last = ctx.last_steal_from();
        task t{};
        auto end = next(last);
        auto idx = next(last);
        do {
            auto& tgt = (*queues_)[idx];
            if(tgt.active() && tgt.try_pop(t)) {
                ctx.last_steal_from(idx);
                ctx.task_is_stolen(true);
                execute_task(t, ctx);
                ctx.task_is_stolen(false);
                return true;
            }
            idx = next(idx);
        } while(idx != end);
        return false;
    }

    void execute_task(task& t, context& ctx) {
        if(! ctx.busy_working()) {
            ++stat_->wakeup_run_;
        }
        ctx.busy_working(true);
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
        ++stat_->count_;
    }

    bool try_process(
        context& ctx,
        basic_queue<task>& q
    ) {
        task t{};
        if (q.active() && q.try_pop(t)) {
            execute_task(t, ctx);
            return true;
        }
        return false;
    }

    bool try_local_and_sticky(
        context& ctx,
        basic_queue<task>& q,
        basic_queue<task>& sq
    ) {
        // sometimes check local queue first for fairness
        auto& notify = ctx.local_first_notifer();
        if(! notify.count_up()) {
            if(try_process(ctx, sq)) {
                ++stat_->sticky_;
                return true;
            }
            if(try_process(ctx, q)) return true;
        } else {
            if(try_process(ctx, q)) return true;
            if(try_process(ctx, sq)) {
                ++stat_->sticky_;
                return true;
            }
        }
        return false;
    }
};

}
