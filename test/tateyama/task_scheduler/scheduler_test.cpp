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
#include <tateyama/api/task_scheduler/scheduler.h>

#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <boost/dynamic_bitset.hpp>

#include <tateyama/api/task_scheduler/basic_task.h>

namespace tateyama::api::task_scheduler {

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace std::chrono_literals;

using namespace testing;

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
    [[nodiscard]] bool delayed() {
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
    [[nodiscard]] bool delayed() {
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
    [[nodiscard]] bool delayed() {
        return false;
    }
    std::function<void(context&)> body_{};
};

class test_task_delayed {
public:
    test_task_delayed() = default;

    explicit test_task_delayed(std::function<void(context&)> body) : body_(std::move(body)) {}
    void operator()(context& ctx) {
        return body_(ctx);
    }
    [[nodiscard]] bool sticky() {
        return false;
    }
    [[nodiscard]] bool delayed() {
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
    w0.init(0);

    context ctx0{0};
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed00);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed01);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed02);
    w0.process_next(ctx0, lq0, sq0);
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
    w0.init(0);
    w1.init(1);

    context ctx0{0};
    context ctx1{1};
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed00);
    w1.process_next(ctx1, lq1, sq1);
    EXPECT_TRUE(executed10);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed11);
}

TEST_F(scheduler_test, delayed_tasks_only) {
    // verify delayed tasks are processed after failing to find STQ/LTX
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky, test_task_delayed>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.ratio_check_local_first({1, 2}); // sticky task and local queue task are processed alternately
    cfg.frequency_promoting_delayed({1, 3}); // checking 3 times, one delayed task is promoted
    cfg.lazy_worker(true);
    cfg.stealing_wait(0);
    scheduler<task> sched{cfg, true};

    auto& w0 = sched.workers()[0];
    auto& lq0 = sched.queues()[0];
    auto& sq0 = sched.sticky_task_queues()[0];
    bool executed00 = false;
    bool executed01 = false;
    sched.schedule_at(task{test_task_delayed{[&](context& t) {
        executed00 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_delayed{[&](context& t) {
        executed01 = true;
    }}}, 0);
    w0.init(0);

    context ctx0{0};
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0)); // when checking 3-times, delayed task is promoted
    EXPECT_TRUE(executed00);
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0)); // another delayed task is promoted
    EXPECT_TRUE(executed01);
}

TEST_F(scheduler_test, sticky_tasks_delayed_tasks) {
    // verify sticky task, local task and delayed tasks are processed in expected order
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky, test_task_delayed>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.ratio_check_local_first({1, 2}); // sticky task and local queue task are processed alternately
    cfg.frequency_promoting_delayed({1, 6}); // when task is checked 6 times, one delayed task is promoted to local
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
    bool executed06 = false;
    bool executed07 = false;
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
    sched.schedule_at(task{test_task_delayed{[&](context& t) {
        executed06 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_delayed{[&](context& t) {
        executed07 = true;
    }}}, 0);
    w0.init(0);

    context ctx0{0};
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed00);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed01);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed02);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed03);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed04);
    w0.process_next(ctx0, lq0, sq0); // delayed task is promoted, but it's not executed instantly
    EXPECT_TRUE(executed05);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_TRUE(executed06);

    w0.process_next(ctx0, lq0, sq0);
    EXPECT_FALSE(executed07);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_FALSE(executed07);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_FALSE(executed07);
    w0.process_next(ctx0, lq0, sq0);
    EXPECT_FALSE(executed07);
    w0.process_next(ctx0, lq0, sq0); // 6 times checked on task, so another delayed task is promoted
    EXPECT_TRUE(executed07);
}

class test_task_sticky_delayed {
public:
    test_task_sticky_delayed() = default;

    explicit test_task_sticky_delayed(std::function<void(context&)> body) : body_(std::move(body)) {}
    void operator()(context& ctx) {
        return body_(ctx);
    }
    [[nodiscard]] bool sticky() {
        return true;
    }
    [[nodiscard]] bool delayed() {
        return true;
    }
    std::function<void(context&)> body_{};
};

TEST_F(scheduler_test, task_sticky_and_delayed) {
    // verify sticky task, local task and delayed tasks are processed in expected order
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky, test_task_delayed, test_task_sticky_delayed>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.ratio_check_local_first({1, 2}); // sticky task and local queue task are processed alternately
    cfg.frequency_promoting_delayed({1, 2}); // when task is checked 3 times, one delayed task is promoted
    scheduler<task> sched{cfg, true};

    auto& w0 = sched.workers()[0];
    auto& lq0 = sched.queues()[0];
    auto& sq0 = sched.sticky_task_queues()[0];
    bool executed00 = false;
    bool executed01 = false;
    bool executed02 = false;
    bool executed03 = false;
    sched.schedule_at(task{test_task_sticky_delayed{[&](context& t) {
        executed03 = true;
    }}}, 0);
    sched.schedule_at(task{test_task{[&](context& t) {
        executed01 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed00 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky{[&](context& t) {
        executed02 = true;
    }}}, 0);
    w0.init(0);

    EXPECT_EQ(2, sq0.size());
    context ctx0{0};
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(executed00);
    EXPECT_EQ(1, sq0.size());
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));  // sticky delayed task is promoted to sticky, but existing sticky is executed first
    EXPECT_TRUE(executed01);
    EXPECT_EQ(2, sq0.size());
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(executed02);
    EXPECT_EQ(1, sq0.size());
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_TRUE(executed03);
    EXPECT_EQ(0, sq0.size());
}
TEST_F(scheduler_test, task_sticky_and_delayed_only) {
    // verify sticky task, local task and delayed tasks are processed in expected order
    using task = tateyama::task_scheduler::basic_task<test_task, test_task_sticky, test_task_delayed, test_task_sticky_delayed>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.ratio_check_local_first({1, 2}); // sticky task and local queue task are processed alternately
    cfg.frequency_promoting_delayed({1, 2}); // when task is checked 3 times, one delayed task is promoted
    cfg.lazy_worker(true);
    cfg.stealing_wait(0);
    scheduler<task> sched{cfg, true};

    auto& w0 = sched.workers()[0];
    auto& lq0 = sched.queues()[0];
    auto& sq0 = sched.sticky_task_queues()[0];
    bool executed00 = false;
    bool executed01 = false;
    sched.schedule_at(task{test_task_sticky_delayed{[&](context& t) {
        executed00 = true;
    }}}, 0);
    sched.schedule_at(task{test_task_sticky_delayed{[&](context& t) {
        executed01 = true;
    }}}, 0);
    w0.init(0);

    context ctx0{0};
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0)); // by existence check, 01 comes first (i.e. not FIFO)
    EXPECT_FALSE(executed01);
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));  // sticky delayed task is promoted and executed
    EXPECT_TRUE(executed01);
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));
    EXPECT_FALSE(executed00);
    EXPECT_TRUE(w0.process_next(ctx0, lq0, sq0));  // sticky delayed task is promoted and executed
    EXPECT_TRUE(executed00);
}

}
