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
    auto cfg = api::configuration::create_configuration("");
    auto section = cfg->get_section("data_store"); // any section name is fine for testing
    ASSERT_TRUE(section);
    ASSERT_TRUE(section->set("test", "value"));
    std::string v{};
    ASSERT_TRUE(section->get("test", v));
    EXPECT_EQ("value", v);
    ASSERT_FALSE(section->set("test", "value2"));
    ASSERT_TRUE(section->get("test", v));
    EXPECT_EQ("value2", v);

    auto section2 = cfg->get_section("data_store");
    std::string v2{};
    ASSERT_TRUE(section2->get("test", v2));
    EXPECT_EQ("value2", v2);
}

TEST_F(configuration_test, add_new_property_bool) {
    auto cfg = api::configuration::create_configuration("");
    auto section = cfg->get_section("data_store"); // any section name is fine for testing
    ASSERT_TRUE(section);
    ASSERT_TRUE(section->set("test", "true"));
    bool v{};
    EXPECT_TRUE(section->get("test", v));
    EXPECT_TRUE(v);
    ASSERT_FALSE(section->set("test", "false"));
    EXPECT_TRUE(section->get("test", v));
    EXPECT_FALSE(v);
}

TEST_F(configuration_test, add_same_name_property_to_different_section) {
    auto cfg = api::configuration::create_configuration("");
    auto section0 = cfg->get_section("data_store"); // any section name is fine for testing
    auto section1 = cfg->get_section("sql"); // any section name is fine for testing
    ASSERT_TRUE(section0);
    ASSERT_TRUE(section1);
    ASSERT_TRUE(section0->set("test", "v0"));
    ASSERT_TRUE(section1->set("test", "v1"));
    std::string v0{}, v1{};
    ASSERT_TRUE(section0->get("test", v0));
    ASSERT_TRUE(section1->get("test", v1));
    EXPECT_EQ("v0", v0);
    EXPECT_EQ("v1", v1);
}

}
