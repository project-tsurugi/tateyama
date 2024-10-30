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

#include <chrono>
#include <sched.h>

#include <tateyama/logging.h>
#include <tateyama/task_scheduler/impl/worker.h>
#include <tateyama/task_scheduler/impl/conditional_worker.h>
#include <tateyama/task_scheduler/basic_conditional_task.h>
#include <tateyama/task_scheduler/impl/queue.h>
#include <tateyama/task_scheduler/impl/thread_control.h>
#include <tateyama/utils/cache_align.h>
#include "task_scheduler_cfg.h"
#include "schedule_option.h"

namespace tateyama::task_scheduler {

/**
 * @brief stealing based task scheduler
 * @tparam T the task type. See comments for `task`.
 * @tparam S the conditional task type.
 */
template <class T, class S = tateyama::task_scheduler::basic_conditional_task>
class cache_align scheduler {
public:
    // clock used for stats
    using clock = std::chrono::steady_clock;

    /**
     * @brief task type scheduled by this
     * @details the task object must be default constructible, move constructible, and move assignable.
     * Interaction with local task queues are done in move semantics.
     */
    using task = T;

    /**
     * @brief conditional task type scheduled by this
     * @details the conditional task object with check() and operator()() member function that provide condition check
     * function and task body. See `basic_conditional_task` for a prototype of conditional task.
     * The task must be default constructible, move constructible, and move assignable.
     * Interaction with conditional task queues are done in move semantics.
     */
    using conditional_task = S;

    /**
     * @brief queue type
     */
    using queue = tateyama::task_scheduler::impl::basic_queue<task>;

    /**
     * @brief conditional task queue type
     */
    using conditional_task_queue = tateyama::task_scheduler::impl::basic_queue<conditional_task>;

    /**
     * @brief worker type
     */
    using worker = tateyama::task_scheduler::impl::worker<task>;

    /**
     * @brief conditional task worker type
     */
    using conditional_worker = tateyama::task_scheduler::impl::conditional_worker<conditional_task>;

    /**
     * @brief copy construct
     */
    scheduler(scheduler const&) = delete;

    /**
     * @brief move construct
     */
    scheduler(scheduler &&) = delete;

    /**
     * @brief copy assign
     */
    scheduler& operator=(scheduler const&) = delete;

    /**
     * @brief move assign
     */
    scheduler& operator=(scheduler &&) = delete;

    /**
     * @brief destruct scheduler
     */
    ~scheduler() = default;

    /**
     * @brief construct new object
     * @param cfg the configuration for this task scheduler
     */
    explicit scheduler(task_scheduler_cfg cfg = {}) :
        cfg_(cfg),
        size_(cfg_.thread_count())
    {
        prepare();
    }

    /**
     * @brief accessor for the preferred worker id
     * @details scheduler has the preference on worker id determined by the caller's thread. This function exposes
     * one to the caller.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    std::size_t preferred_worker_for_current_thread() {
        return set_preferred_worker_for_current_thread(undefined);
    }

    /**
     * @brief setter of the preferred worker for the current thread
     * @details scheduler has the preference on worker id determined by the caller's thread. This function sets
     * the preference for the calling thread.
     * @note this function is not thread-safe. Expected to be called only once at the worker initialization.
     */
    void initialize_preferred_worker_for_current_thread(std::size_t worker_index) {
        set_preferred_worker_for_current_thread(worker_index);
    }

    /**
     * @brief schedule conditional task
     * @param t the task to be scheduled.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    void schedule_conditional(conditional_task && t) {
        conditional_queue_.push(std::move(t));
        if(watcher_thread_) {
            watcher_thread_->activate();
        }
    }

    /**
     * @brief schedule task
     * @param t the task to be scheduled.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    void schedule(task&& t, schedule_option opt = {}) {
        auto index = select_worker(opt);
        schedule_at(std::move(t), index);
    }

    /**
     * @brief schedule task on the specified worker
     * @param t the task to be scheduled.
     * @param index the preferred worker index for the task to execute. This puts the task on the queue that the specified
     * worker has, but doesn't ensure the task to be run by the worker if stealing happens.
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    void schedule_at(task&& t, std::size_t index) {
        BOOST_ASSERT(index < size_); //NOLINT
        auto& thread = threads_[index];
        if (! started_) {
            auto& s = initial_tasks_[index];
            s.emplace_back(std::move(t));
            return;
        }
        if(t.sticky()) {
            auto& q = sticky_task_queues_[index];
            q.push(std::move(t));
            if(! cfg_.busy_worker()) {
                thread.activate();
            }
            return;
        }
        auto& q = queues_[index];
        q.push(std::move(t));
        if(! cfg_.busy_worker()) {
            thread.activate();
        }
    }

    /**
     * @brief start the scheduler
     * @details start the scheduler
     * @note this function is *NOT* thread-safe. Only a thread must call this before using the scheduler.
     */
    void start() {
        for(auto&& t : threads_) {
            t.wait_initialization();
        }
        if(watcher_thread_) {
            watcher_thread_->wait_initialization();
        }

        for(auto&& t : threads_) {
            t.activate();
        }
        if(watcher_thread_) {
            watcher_thread_->activate();
        }
        started_at_ = clock::now();
        started_ = true;
    }

    /**
     * @brief stop the scheduler
     * @details stop the scheduler and join the worker threads
     * @note this function is *NOT* thread-safe. Only a thread must call this when finishing using the scheduler.
     */
    void stop() {
        for(auto&& q : queues_) {
            q.deactivate();
        }
        for(auto&& q : sticky_task_queues_) {
            q.deactivate();
        }
        conditional_queue_.deactivate();
        if(watcher_thread_) {
            ensure_stopping_thread(*watcher_thread_);
        }

        for(auto&& t : threads_) {
            ensure_stopping_thread(t);
        }
        started_ = false;
    }

    /**
     * @brief accessor to the worker count
     * @return the number of worker (threads and queues)
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }
    /**
     * @brief accessor to the worker statistics
     * @note this function is thread-safe. Multiple threads can safely call this function concurrently.
     */
    [[nodiscard]] std::vector<tateyama::task_scheduler::impl::worker_stat> const& worker_stats() const noexcept {
        return worker_stats_;
    }

    /**
     * @brief accessor to the contexts for testing purpose
     */
    [[nodiscard]] std::vector<context>& contexts() noexcept {
        return contexts_;
    }

    /**
     * @brief accessor to the local queue for testing purpose
     */
    [[nodiscard]] std::vector<queue>& queues() noexcept {
        return queues_;
    }

    /**
     * @brief accessor to the sticky task queue for testing purpose
     */
    [[nodiscard]] std::vector<queue>& sticky_task_queues() noexcept {
        return sticky_task_queues_;
    }

    /**
     * @brief accessor to the workers for testing purpose
     * @return the workers list
     */
    [[nodiscard]] std::vector<worker>& workers() noexcept {
        return workers_;
    }

    /**
     * @brief accessor to the conditional worker for testing purpose
     * @return the conditional worker
     */
    [[nodiscard]] conditional_worker& cond_worker() noexcept {
        return conditional_worker_;
    }

    /**
     * @brief accessor to the conditional queue for testing purpose
     * @return the conditional queue
     */
    [[nodiscard]] conditional_task_queue & cond_queue() noexcept {
        return conditional_queue_;
    }

    /**
     * @brief accessor to the conditional context for testing purpose
     * @return the conditional worker context
     */
    [[nodiscard]] tateyama::task_scheduler::impl::conditional_worker_context const& conditional_worker_context() const noexcept {
        return conditional_worker_context_;
    }

    /**
     * @brief accessor to the threads for testing purpose
     */
    [[nodiscard]] std::vector<impl::thread_control>& threads() noexcept {
        return threads_;
    }

    /**
     * @brief print diagnostics
     */
    void print_diagnostic(std::ostream& os) {
        if (! started_) {
            // print nothing if not started yet
            return;
        }
        auto count = contexts_.size();
        os << "worker_count: " << count << std::endl;
        os << "workers:" << std::endl;
        for(std::size_t i=0; i<count; ++i) {
            os << "  - worker_index: " << i << std::endl;
            os << "    thread: " << std::endl;
            threads_[i].print_diagnostic(os);
            os << "    queues:" << std::endl;
            os << "      local:" << std::endl;
            print_queue_diagnostic(queues_[i], os);
            os << "      sticky:" << std::endl;
            print_queue_diagnostic(sticky_task_queues_[i], os);
        }
        // TODO fix indent for conditional
        os << "conditional_worker:" << std::endl;
        os << "  thread: " << std::endl;
        watcher_thread_->print_diagnostic(os);
        os << "  queue:" << std::endl;
        print_queue_diagnostic(conditional_queue_, os);
    }

    /**
     * @brief select the worker to schedule the task
     * @note this function should be private, but kept public for testing purpose
     */
    std::size_t select_worker(schedule_option const& opt) {
        std::size_t index =
            cfg_.use_preferred_worker_for_current_thread() ? preferred_worker_for_current_thread() : next_worker();
        if (opt.policy() == schedule_policy_kind::undefined) {
            return index;
        }

        // opt.policy() == schedule_policy_kind::suspended_worker

        // candidate worker is likely to be heavily used (esp. when preferred for current thread),
        // so find beginning from the next
        auto base = next(index, size_);
        auto cur = base;
        do {
            if(! threads_[cur].active()) {
                break;
            }
            cur = next(cur, size_);
        } while(cur != base);
        return cur;
    }

    /**
     * @brief retrieve the candidate index for next worker (round-robbin) and atomically increment for later use
     */
    std::size_t next_worker() {
        auto n = next_worker_index_before_modulo_++;
        return n % size_;
    }

    /**
     * @brief setter for the next worker index for testing purpose
     */
    void next_worker(std::size_t arg) {
        next_worker_index_before_modulo_ = arg;
    }

    constexpr static auto undefined = static_cast<std::size_t>(-1);

    /**
     * @brief setter for the preferred worker index for testing purpose
     */
    std::size_t set_preferred_worker_for_current_thread(std::size_t index = undefined) {
        thread_local std::size_t index_for_this_thread = undefined;
        if (index != undefined) {
            index_for_this_thread = index;
            return index_for_this_thread;
        }
        if (index_for_this_thread == undefined) {
            index_for_this_thread = next_worker();
            // using VLOG since caller threads are frequently re-created and this message is displayed often
            VLOG_LP(log_debug) << "worker " << index_for_this_thread << " assigned for thread on core " << sched_getcpu();
        }
        return index_for_this_thread;
    }

    /**
     * @brief print worker stats
     */
    void print_worker_stats(std::ostream& os) {
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - started_at_).count();
        auto count = contexts_.size();
        os << "{";
        os << "\"duration_us\":" << duration_us << ",";
        os << "\"worker_count\":" << count << ",";
        os << "\"workers\":[";
        for(std::size_t i=0; i < count; ++i) {
            auto& stat = worker_stats_[i];
            if(i != 0) {
                os << ",";
            }
            os << "{";
            os << "\"worker_index\":" << i << ",";
            os << "\"count\":" << stat.count_ << ",";
            os << "\"sticky\":" << stat.sticky_ << ",";
            os << "\"steal\":" << stat.steal_ << ",";
            os << "\"wakeup_run\":" << stat.wakeup_run_ << ",";
            os << "\"suspend\":" << stat.suspend_;
            os << "}";
        }
        os << "]";
        os << "}";
    }
private:
    task_scheduler_cfg cfg_{};
    std::size_t size_{};
    std::vector<queue> queues_{};
    std::vector<queue> sticky_task_queues_{};
    std::vector<worker> workers_{};  // stored for testing
    std::vector<impl::thread_control> threads_{};
    std::vector<impl::worker_stat> worker_stats_{};
    std::vector<context> contexts_{};
    std::atomic_size_t next_worker_index_before_modulo_{};
    std::vector<std::vector<task>> initial_tasks_{};
    std::atomic_bool started_{false};
    conditional_task_queue conditional_queue_{};
    // use unique_ptr to avoid default constructor to spawn new thread
    std::unique_ptr<impl::thread_control> watcher_thread_{};
    impl::conditional_worker_context conditional_worker_context_{};
    conditional_worker conditional_worker_{}; // stored for testing
    clock::time_point started_at_{};

    void prepare() {
        auto sz = cfg_.thread_count();
        queues_.resize(sz);
        sticky_task_queues_.resize(sz);
        worker_stats_.resize(sz);
        initial_tasks_.resize(sz);
        contexts_.reserve(sz);
        workers_.reserve(sz);
        threads_.reserve(sz);
        for(std::size_t i = 0; i < sz; ++i) {
            auto& ctx = contexts_.emplace_back(i);
            ctx.local_first_notifer().init(
                static_cast<std::size_t>(cfg_.ratio_check_local_first().numerator()),
                static_cast<std::size_t>(cfg_.ratio_check_local_first().denominator())
            );
            auto& worker = workers_.emplace_back(
                queues_, sticky_task_queues_, initial_tasks_, worker_stats_[i], cfg_, [this](std::size_t index) {
                        this->initialize_preferred_worker_for_current_thread(index);
                });
            if (cfg_.empty_thread()) {
                threads_.emplace_back(); // for testing
            } else {
                threads_.emplace_back(i, std::addressof(cfg_), worker, ctx);
            }
        }
        conditional_worker_ = conditional_worker{conditional_queue_, cfg_};
        if (! cfg_.empty_thread()) {
            watcher_thread_ = std::make_unique<impl::thread_control>(
                impl::thread_control::undefined_thread_id,
                std::addressof(cfg_),
                conditional_worker_,
                conditional_worker_context_
            );
        }
    }

    std::size_t next(std::size_t index, std::size_t mod) {
        if (index == mod - 1) {
            return 0;
        }
        return index + 1;
    }

    /**
     * @brief print queue diagnostics
     */
    template<class Queue>
    void print_queue_diagnostic(Queue& q, std::ostream& os) {
        os << "        task_count: " << q.size() << std::endl;
        if(q.empty()) {
            return;
        }
        os << "        tasks:" << std::endl;
        Queue backup{};
        typename Queue::task t{};
        while(q.try_pop(t)) {
            print_task_diagnostic(t, os);
            backup.push(std::move(t));
        }
        while(backup.try_pop(t)) {
            q.push(std::move(t));
        }
    }

    void ensure_stopping_thread(impl::thread_control& th) {
        while(! th.completed()) {
            // in case thread suspends on cv, activate and wait for completion
            th.activate();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        th.join();
    }

    static_assert(std::is_default_constructible_v<task>);
    static_assert(std::is_move_constructible_v<task>);
    static_assert(std::is_move_assignable_v<task>);
};

}

