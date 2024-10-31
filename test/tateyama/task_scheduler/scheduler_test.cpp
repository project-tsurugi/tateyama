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
#include <tateyama/task_scheduler/scheduler.h>

#include <chrono>
#include <future>
#include <thread>
#include <boost/dynamic_bitset.hpp>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include <tateyama/task_scheduler/basic_conditional_task.h>
#include <tateyama/task_scheduler/basic_task.h>
#include <tateyama/task_scheduler/impl/thread_initialization_info.h>

namespace tateyama::task_scheduler {

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace std::chrono_literals;

using namespace testing;
using impl::thread_initialization_info;

class scheduler_test : public ::testing::Test {
public:
};

class test_task {
public:
    test_task() = default;

    explicit test_task(std::function<void(context&)> body) : body_(std::move(body)) {}
    void operator()(context& ctx) {
        return body_(ctx);
    }
    [[nodiscard]] bool sticky() {
        return false;
    }
    std::function<void(context&)> body_{};
};

TEST_F(scheduler_test, basic) {
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    scheduler<test_task> sched{cfg};
    bool executed = false;
    test_task t{[&](context& t) {
        executed = true;
    }};
    sched.start();
    std::this_thread::sleep_for(100ms);
    sched.schedule(std::move(t));
    std::this_thread::sleep_for(100ms);
    sched.stop();
    ASSERT_TRUE(executed);
}

class test_task2 {
public:
    test_task2() = default;

    explicit test_task2(std::function<void(context&)> body) : body_(std::move(body)) {}
    void operator()(context& ctx) {
        return body_(ctx);
    }
    [[nodiscard]] bool sticky() {
        return false;
    }
    std::function<void(context&)> body_{};
};

TEST_F(scheduler_test, multiple_task_impls) {
    using task = tateyama::task_scheduler::basic_task<test_task, test_task2>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    scheduler<task> sched{cfg};
    bool executed = false;
    bool executed2 = false;
    test_task t{[&](context& t) {
        executed = true;
    }};
    test_task2 t2{[&](context& t) {
        executed2 = true;
    }};
    sched.start();
    std::this_thread::sleep_for(100ms);
    sched.schedule(task{std::move(t)});
    sched.schedule(task{std::move(t2)});
    std::this_thread::sleep_for(100ms);
    sched.stop();
    ASSERT_TRUE(executed);
    ASSERT_TRUE(executed2);
}

class test_task_sticky {
public:
    test_task_sticky() = default;

    explicit test_task_sticky(std::function<void(context&)> body) : body_(std::move(body)) {}
    void operator()(context& ctx) {
        return body_(ctx);
    }
    [[nodiscard]] bool sticky() {
        return true;
    }
    std::function<void(context&)> body_{};
};

TEST_F(scheduler_test, sticky_task_simple) {
    // verify sticky task and local queue task are all processed in expected order
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.ratio_check_local_first({1, 2}); // sticky task and local queue task are processed alternately
    cfg.empty_thread(true);
    scheduler<task> sched{cfg};

    auto& w0 = sched.workers()[0];
    auto& lq0 = sched.queues()[0];
    auto& sq0 = sched.sticky_task_queues()[0];
    bool executed00 = false;
    bool executed01 = false;
    bool executed02 = false;
    bool executed03 = false;
    sched.schedule_at(task{test_task{[&](context& t) {
        executed01 = true;
    }}}, 0);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed03 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed00 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed02 = true;
    }}}, 0);
    auto& ctx = sched.contexts()[0];
    w0.init(thread_initialization_info{0}, ctx);

    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed00);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed01);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed02);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed03);
}

TEST_F(scheduler_test, sticky_task_stealing) {
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(2);
    cfg.empty_thread(true);
    scheduler<task> sched{cfg};

    auto& w0 = sched.workers()[0];
    auto& w1 = sched.workers()[1];
    auto& lq0 = sched.queues()[0];
    auto& lq1 = sched.queues()[1];
    auto& sq0 = sched.sticky_task_queues()[0];
    auto& sq1 = sched.sticky_task_queues()[1];
    bool executed00 = false;
    bool executed10 = false;
    bool executed11 = false;
    sched.schedule_at(task{test_task{[&](context& t) {
        executed00 = true;
    }}}, 0);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed11 = true;
    }}}, 1);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed10 = true;
    }}}, 1);
    auto& ctx = sched.contexts()[0];
    w0.init(thread_initialization_info{0}, ctx);
    w0.init(thread_initialization_info{1}, ctx);

    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed00);
    w1.process_next(ctx, lq1, sq1);
    EXPECT_TRUE(executed10);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed11);
}

TEST_F(scheduler_test, sticky_tasks) {
    // verify sticky task and local task are processed in expected order
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.ratio_check_local_first({1, 2}); // sticky task and local queue task are processed alternately
    cfg.stealing_wait(0);
    cfg.empty_thread(true);
    scheduler<task> sched{cfg};

    auto& w0 = sched.workers()[0];
    auto& lq0 = sched.queues()[0];
    auto& sq0 = sched.sticky_task_queues()[0];
    bool executed00 = false;
    bool executed01 = false;
    bool executed02 = false;
    bool executed03 = false;
    bool executed04 = false;
    bool executed05 = false;
    sched.schedule_at(task{test_task{[&](context& t) {
        executed01 = true;
    }}}, 0);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed03 = true;
    }}}, 0);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed05 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed00 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed02 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed04 = true;
    }}}, 0);
    auto& ctx = sched.contexts()[0];
    w0.init(thread_initialization_info{0}, ctx);

    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed00);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed01);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed02);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed03);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed04);
    w0.process_next(ctx, lq0, sq0);
    EXPECT_TRUE(executed05);
    w0.process_next(ctx, lq0, sq0);
}

TEST_F(scheduler_test, stealing_from_last_worker) {
    // regression testcase for issue #801 - worker failed to steal from the worker at the last position
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(5);
    cfg.stealing_wait(0);
    cfg.empty_thread(true);
    scheduler<task> sched{cfg};

    auto& w0 = sched.workers()[0];
    auto& w1 = sched.workers()[1];
    auto& w2 = sched.workers()[2];
    auto& w3 = sched.workers()[3];
    auto& w4 = sched.workers()[4];
    auto& lq0 = sched.queues()[0];
    auto& sq0 = sched.sticky_task_queues()[0];
    bool executed01 = false;
    bool executed02 = false;
    bool executed03 = false;
    bool executed04 = false;
    sched.schedule_at(task{test_task{[&](context& t) {
        executed01 = true;
    }}}, 1);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed02 = true;
    }}}, 2);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed03 = true;
    }}}, 3);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed04 = true;
    }}}, 4);

    auto& ctx0 = sched.contexts()[0];
    auto& ctx1 = sched.contexts()[1];
    auto& ctx2 = sched.contexts()[2];
    auto& ctx3 = sched.contexts()[3];
    auto& ctx4 = sched.contexts()[4];
    w0.init(thread_initialization_info{0}, ctx0);
    w1.init(thread_initialization_info{1}, ctx1);
    w2.init(thread_initialization_info{2}, ctx2);
    w3.init(thread_initialization_info{3}, ctx3);
    w4.init(thread_initialization_info{4}, ctx4);
    ctx0.last_steal_from(2);

    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed03);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed04);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed01);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed02);
}


TEST_F(scheduler_test, select_worker_prefered_for_current_thread) {
    // verify select_worker() returns preferred worker for current thread
    task_scheduler_cfg cfg{};
    cfg.thread_count(5);
    cfg.empty_thread(true);
    cfg.use_preferred_worker_for_current_thread(true);
    scheduler<test_task> sched{cfg};
    sched.set_preferred_worker_for_current_thread(2);
    schedule_option opt{schedule_policy_kind::undefined};
    EXPECT_EQ(2, sched.select_worker(opt));
    EXPECT_EQ(2, sched.select_worker(opt));
    auto f = std::async(std::launch::async, [&] {
        sched.set_preferred_worker_for_current_thread(3);
        EXPECT_EQ(3, sched.select_worker(opt));
    });
    f.get();
    EXPECT_EQ(2, sched.select_worker(opt));
}

TEST_F(scheduler_test, select_worker_by_round_robbin) {
    // verify select_worker() returns round-robbin worker index
    task_scheduler_cfg cfg{};
    cfg.thread_count(5);
    cfg.empty_thread(true);
    cfg.use_preferred_worker_for_current_thread(false);
    scheduler<test_task> sched{cfg};
    sched.next_worker(3);
    schedule_option opt{schedule_policy_kind::undefined};
    EXPECT_EQ(3, sched.select_worker(opt));
    EXPECT_EQ(4, sched.select_worker(opt));
    EXPECT_EQ(0, sched.select_worker(opt));
    EXPECT_EQ(1, sched.select_worker(opt));
    EXPECT_EQ(2, sched.select_worker(opt));
}

TEST_F(scheduler_test, select_suspended_worker) {
    // verify select_worker() returns suspended worker index
    // check workers 3, 4, 0, 1, 2 in this order
    task_scheduler_cfg cfg{};
    cfg.thread_count(5);
    cfg.empty_thread(true);
    cfg.use_preferred_worker_for_current_thread(true);
    scheduler<test_task> sched{cfg};
    auto& threads = sched.threads();
    sched.set_preferred_worker_for_current_thread(2);
    {
        // all threads suspended - use next to preferred
        threads[0].set_active(false);
        threads[1].set_active(false);
        threads[2].set_active(false);
        threads[3].set_active(false);
        threads[4].set_active(false);
        schedule_option opt{schedule_policy_kind::suspended_worker};
        EXPECT_EQ(3, sched.select_worker(opt));
    }
    {
        // all threads active - use next to preferred
        threads[0].set_active(true);
        threads[1].set_active(true);
        threads[2].set_active(true);
        threads[3].set_active(true);
        threads[4].set_active(true);
        schedule_option opt{schedule_policy_kind::suspended_worker};
        EXPECT_EQ(3, sched.select_worker(opt));
    }
    {
        // 3, 4, 0 threads are active
        threads[0].set_active(true);
        threads[1].set_active(false);
        threads[2].set_active(false);
        threads[3].set_active(true);
        threads[4].set_active(true);
        schedule_option opt{schedule_policy_kind::suspended_worker};
        EXPECT_EQ(1, sched.select_worker(opt));
    }
    {
        // 3, 4, 0, 1 threads are active
        threads[0].set_active(true);
        threads[1].set_active(true);
        threads[2].set_active(false);
        threads[3].set_active(true);
        threads[4].set_active(true);
        schedule_option opt{schedule_policy_kind::suspended_worker};
        EXPECT_EQ(2, sched.select_worker(opt));
    }
}

TEST_F(scheduler_test, thread_initializer) {
    task_scheduler_cfg cfg{};
    cfg.thread_count(3);
    std::vector<std::size_t> indices{};
    bool executed = false;
    scheduler<test_task> sched{cfg, [&](std::size_t index) {
        executed = true;
        indices.emplace_back(index);
    }};

    // start/stop to ensure all threads initializer
    sched.start();
    sched.stop();
    EXPECT_EQ(3, indices.size());
    std::sort(indices.begin(), indices.end());
    EXPECT_EQ(0, indices[0]);
    EXPECT_EQ(1, indices[1]);
    EXPECT_EQ(2, indices[2]);
    EXPECT_TRUE(executed);
}


}
