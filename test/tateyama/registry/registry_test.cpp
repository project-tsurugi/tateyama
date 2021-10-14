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
#include <tateyama/api/registry/registry.h>

#include <regex>
#include <gtest/gtest.h>

#include "test_component1.h"
#include "test_component2.h"

namespace tateyama::api::registry {

using namespace std::literals::string_literals;

class registry_test : public ::testing::Test {

};

using namespace std::string_view_literals;

TEST_F(registry_test, basic) {
    auto impl1 = tateyama::api::registry::registry<common::component1>::create("1_1");
    EXPECT_EQ("test_component11", impl1->run());
    auto impl2 = tateyama::api::registry::registry<common::component1>::create("1_2");
    EXPECT_EQ("test_component12", impl2->run());
}

TEST_F(registry_test, multiple) {
    auto impl11 = tateyama::api::registry::registry<common::component1>::create("1_1");
    EXPECT_EQ("test_component11", impl11->run());
    auto impl12 = tateyama::api::registry::registry<common::component1>::create("1_2");
    EXPECT_EQ("test_component12", impl12->run());
    auto impl21 = tateyama::api::registry::registry<common::component2>::create("2_1");
    EXPECT_EQ(21, impl21->run());
    auto impl22 = tateyama::api::registry::registry<common::component2>::create("2_2");
    EXPECT_EQ(22, impl22->run());
}
}
