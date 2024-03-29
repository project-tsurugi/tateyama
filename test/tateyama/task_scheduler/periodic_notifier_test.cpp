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
#include <tateyama/task_scheduler/impl/periodic_notifier.h>
#include <chrono>
#include <string>
#include <string_view>
#include <gtest/gtest.h>

namespace tateyama::task_scheduler::impl {

using namespace std::literals::string_literals;
using namespace std::chrono_literals;

class periodic_notifier_test : public ::testing::Test {

};

using namespace std::string_view_literals;

TEST_F(periodic_notifier_test, default_instance) {
    periodic_notifier n{};
    EXPECT_TRUE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_TRUE(n.count_up());
}

TEST_F(periodic_notifier_test, simple) {
    periodic_notifier n{1, 2};
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
}

TEST_F(periodic_notifier_test, ratio_2_3) {
    periodic_notifier n{2, 3};
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_TRUE(n.count_up());
}

TEST_F(periodic_notifier_test, ratio_1_3) {
    periodic_notifier n{1, 3};
    EXPECT_FALSE(n.count_up());
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
    EXPECT_FALSE(n.count_up());
    EXPECT_FALSE(n.count_up());
    EXPECT_TRUE(n.count_up());
}

TEST_F(periodic_notifier_test, reset) {
    periodic_notifier n{1, 2};
    EXPECT_FALSE(n.count_up());
    n.reset();
    EXPECT_FALSE(n.count_up());
    n.reset();
    EXPECT_FALSE(n.count_up());
    n.reset();
}

}
