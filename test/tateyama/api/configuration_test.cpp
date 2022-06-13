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
#include <tateyama/api/configuration.h>

#include <gtest/gtest.h>

namespace tateyama::api {

using namespace std::literals::string_literals;

class configuration_test : public ::testing::Test {
public:

};

using namespace std::string_view_literals;

TEST_F(configuration_test, add_new_property) {
    auto cfg = api::configuration::create_configuration();
    auto section = cfg->get_section("datastore"); // any section name is fine for testing
    ASSERT_TRUE(section);
    ASSERT_TRUE(section->set("test", "value"));
    auto v1 = section->get<std::string>("test");
    ASSERT_TRUE(v1);
    EXPECT_EQ("value", v1.value());
    ASSERT_FALSE(section->set("test", "value2"));
    auto vu = section->get<std::string>("test");
    ASSERT_TRUE(vu);
    EXPECT_EQ("value2", vu.value());

    auto section2 = cfg->get_section("datastore");
    auto v2 = section2->get<std::string>("test");
    ASSERT_TRUE(v2);
    EXPECT_EQ("value2", v2.value());
}

TEST_F(configuration_test, add_new_property_bool) {
    auto cfg = api::configuration::create_configuration();
    auto section = cfg->get_section("datastore"); // any section name is fine for testing
    ASSERT_TRUE(section);
    ASSERT_TRUE(section->set("test", "true"));
    auto v = section->get<bool>("test");
    EXPECT_TRUE(v);
    EXPECT_TRUE(v.value());
    ASSERT_FALSE(section->set("test", "false"));
    auto vf = section->get<bool>("test");
    EXPECT_TRUE(vf);
    EXPECT_FALSE(vf.value());
}

TEST_F(configuration_test, add_same_name_property_to_different_section) {
    auto cfg = api::configuration::create_configuration();
    auto section0 = cfg->get_section("datastore"); // any section name is fine for testing
    auto section1 = cfg->get_section("sql"); // any section name is fine for testing
    ASSERT_TRUE(section0);
    ASSERT_TRUE(section1);
    ASSERT_TRUE(section0->set("test", "v0"));
    ASSERT_TRUE(section1->set("test", "v1"));
    auto v0 = section0->get<std::string>("test");
    auto v1 = section1->get<std::string>("test");
    ASSERT_TRUE(v0);
    ASSERT_TRUE(v1);
    EXPECT_EQ("v0", v0.value());
    EXPECT_EQ("v1", v1.value());
}

}
