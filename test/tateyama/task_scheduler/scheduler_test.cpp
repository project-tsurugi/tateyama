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
#include <thread>

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <boost/dynamic_bitset.hpp>

#include <tateyama/task_scheduler/basic_task.h>
#include <tateyama/task_scheduler/basic_conditional_task.h>
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
    scheduler<task> sched{cfg, true};

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
    scheduler<task> sched{cfg, true};

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
    scheduler<task> sched{cfg, true};

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

}
