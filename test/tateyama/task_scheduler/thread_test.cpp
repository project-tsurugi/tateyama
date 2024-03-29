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
#include <tateyama/task_scheduler/impl/thread_control.h>

#include <regex>
#include <xmmintrin.h>
#include <gtest/gtest.h>

#include <thread>
#include <future>

#include <tateyama/task_scheduler/impl/thread_initialization_info.h>

namespace tateyama::task_scheduler::impl {

using namespace std::literals::string_literals;
using namespace std::chrono_literals;

class thread_test : public ::testing::Test {

};

using namespace std::string_view_literals;

TEST_F(thread_test, create_thread) {
    {
        int x = 0;
        thread_control t{[&](){ ++x; }};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
    {
        int x = 0;
        std::function<void(void)> f = [&](){ ++x; };
        thread_control t{f};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
    {
        int x = 0;
        std::function<void(void)> f = [&](){ ++x; };
        thread_control t{std::move(f)};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
}

TEST_F(thread_test, active) {
    bool active = false;
    std::atomic_bool thread_finished{false};
    thread_control t{[&](){
        active = t.active();
        thread_finished.store(true);
    }};
    EXPECT_FALSE(t.active());
    t.activate();
    while(! thread_finished.load()) {}
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(t.active());
    t.join();
    EXPECT_FALSE(t.active());
    EXPECT_TRUE(active);
}

TEST_F(thread_test, task_with_args) {
    {
        int x = 0;
        thread_control t{[&](int y){ ++x; }, 1};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
    {
        int x = 0;
        std::function<void(int)> f = [&](int y){ x += y; };
        thread_control t{f, 1};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
    {
        int x = 0;
        std::function<void(int)> f = [&](int y){ x += y; };
        thread_control t{std::move(f), 1};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
}

TEST_F(thread_test, vector_of_threads) {
    std::vector<thread_control> threads{};
    int x=0;
    threads.emplace_back([&](){ ++x; });
    auto& t = threads[0];
    t.activate();
    t.join();
    EXPECT_EQ(1, x);
}

TEST_F(thread_test, modifying_thread_input) {
    {
        int x = 0;
        thread_control t{[](int& x){ ++x; }, x};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
    {
        int x = 0;
        std::function<void(int& x)> f = [](int &x){ ++x; };
        thread_control t{f, x};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
    {
        int x = 0;
        std::function<void(int& x)> f = [](int &x){ ++x; };
        thread_control t{std::move(f), x};
        t.activate();
        t.join();
        EXPECT_EQ(1, x);
    }
}

TEST_F(thread_test, wait_initialization) {
    // test wait_initialization() correctly wait completion of init()
    struct callable {
        explicit callable(std::atomic_bool& initialized) :
            initialized_(initialized)
        {}

        void init(thread_initialization_info const& info) {
            (void) info;
            while(! initialized_) {
                _mm_pause();
            }
        }
        void set_initialized() {
            initialized_ = true;
        }
        std::atomic_bool& initialized_;
        void operator()() {
            // no-op
        }
    };

    std::atomic_bool initialized = false;
    callable c{initialized};
    task_scheduler_cfg cfg{};
    thread_control t{0, &cfg, c};
    std::this_thread::sleep_for(1ms);
    ASSERT_FALSE(c.initialized_);
    std::atomic_bool wait_completed = false;
    auto f = std::async(std::launch::async, [&]() {
        t.wait_initialization();
        wait_completed = true;
    });
    std::this_thread::sleep_for(1ms);
    ASSERT_FALSE(c.initialized_);
    ASSERT_FALSE(wait_completed);
    c.set_initialized();
    f.get();
    ASSERT_TRUE(c.initialized_);
    ASSERT_TRUE(wait_completed);

    t.activate();
    t.join();
}

}
