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

namespace tateyama::task_scheduler {

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace std::chrono_literals;

using namespace testing;

class conditional_schedule_test : public ::testing::Test {
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

TEST_F(conditional_schedule_test, simple) {
    using task = tateyama::task_scheduler::basic_task<test_task>;
    using conditional_task = tateyama::task_scheduler::basic_conditional_task;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.busy_worker(false);
    scheduler<task, conditional_task> sched{cfg};
    std::atomic_bool executed = false;
    std::atomic_size_t cnt = 0;
    conditional_task t{
        [&]() {
            ++cnt;
            return cnt >= 5;
        },
        [&]() {
            executed = true;
        },
    };
    sched.start();
    std::this_thread::sleep_for(10ms);
    sched.schedule_conditional(std::move(t));
    std::this_thread::sleep_for(10ms);
    sched.stop();
    ASSERT_TRUE(executed);
    ASSERT_EQ(5, cnt);
}

using conditional_task = tateyama::task_scheduler::basic_conditional_task;
conditional_task create_conditional_task(
    std::size_t max,
    std::atomic_bool& executed,
    std::atomic_size_t& cnt
) {
    return conditional_task{
        [&, max]() {
            ++cnt;
            return cnt >= max;
        },
        [&]() {
            executed = true;
        },
    };
}
TEST_F(conditional_schedule_test, step_by_step) {
    // verify conditional worker logic step by step using process_next()
    using task = tateyama::task_scheduler::basic_task<test_task>;
    task_scheduler_cfg cfg{};
    cfg.thread_count(0);
    cfg.busy_worker(false);
    cfg.empty_thread(true);
    scheduler<task, conditional_task> sched{cfg};
    std::atomic_bool executed1 = false;
    std::atomic_size_t cnt1 = 0;
    std::atomic_bool executed2 = false;
    std::atomic_size_t cnt2 = 0;
    auto t1 = create_conditional_task(1, executed1, cnt1);
    auto t2 = create_conditional_task(2, executed2, cnt2);
    sched.start();
    auto& w = sched.cond_worker();
    auto& q = sched.cond_queue();
    impl::conditional_worker_context ctx{};
    sched.schedule_conditional(std::move(t1));
    sched.schedule_conditional(std::move(t2));
    EXPECT_EQ(2, q.size());
    w.process_next();
    EXPECT_TRUE(executed1);
    EXPECT_EQ(1, cnt1);
    EXPECT_EQ(1, q.size());
    EXPECT_FALSE(executed2);
    EXPECT_EQ(1, cnt2);
    w.process_next();
    EXPECT_TRUE(executed2);
    EXPECT_EQ(1, cnt1);
    EXPECT_EQ(2, cnt2);
    EXPECT_EQ(0, q.size());
    sched.stop();
}

TEST_F(conditional_schedule_test, stop_while_cond_task_repeat_checking) {
    // verify stopping task scheduler timely even conditional task remains with large watcher_interval
    using task = tateyama::task_scheduler::basic_task<test_task>;
    using conditional_task = tateyama::task_scheduler::basic_conditional_task;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.busy_worker(false);
    cfg.watcher_interval(2*60*1000*1000);
    scheduler<task, conditional_task> sched{cfg};
    std::atomic_bool executed = false;
    conditional_task t{
        [&]() {
            return false;
        },
        [&]() {
            executed = true;
        },
    };
    sched.start();
    std::this_thread::sleep_for(10ms);
    sched.schedule_conditional(std::move(t));
    std::this_thread::sleep_for(10ms);
    sched.stop();
    ASSERT_FALSE(executed);
}

TEST_F(conditional_schedule_test, stop_with_empty_cond_task) {
    // verify stopping task scheduler timely even there is no conditional task
    using task = tateyama::task_scheduler::basic_task<test_task>;
    using conditional_task = tateyama::task_scheduler::basic_conditional_task;
    task_scheduler_cfg cfg{};
    cfg.thread_count(1);
    cfg.busy_worker(false);
    cfg.watcher_interval(2*60*1000*1000);
    scheduler<task, conditional_task> sched{cfg};
    sched.start();
    std::this_thread::sleep_for(1ms);
    ASSERT_FALSE(sched.conditional_worker_context().thread()->active());
    sched.stop();
}
}
