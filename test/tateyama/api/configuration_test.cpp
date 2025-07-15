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
#include <sstream>
#include <string_view>
#include <vector>

#include <tateyama/api/configuration.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::api {

using namespace std::literals::string_literals;

class configuration_test : public ::testing::Test {
public:

};

using namespace std::string_view_literals;

TEST_F(configuration_test, add_new_property) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
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
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
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

TEST_F(configuration_test, add_new_property_size_t) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    auto section = cfg->get_section("datastore"); // any section name is fine for testing
    ASSERT_TRUE(section);
    ASSERT_TRUE(section->set("test", "100"));
    auto v = section->get<std::size_t>("test");
    EXPECT_EQ(100, v);
    EXPECT_EQ(100, v.value());
    ASSERT_FALSE(section->set("test", "200"));
    auto vf = section->get<std::size_t>("test");
    EXPECT_EQ(200, vf);
    EXPECT_EQ(200, vf.value());
    ASSERT_FALSE(section->set("test", "300"));
    auto ve = section->get<std::size_t>("test");
    EXPECT_EQ(300, ve);
    EXPECT_EQ(300, ve.value());
}

TEST_F(configuration_test, add_same_name_property_to_different_section) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
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

TEST_F(configuration_test, property_file_missing) {
    // even on invalid path, configuration object is created with default values
    auto cfg = api::configuration::create_configuration("/dummy/file/path", tateyama::test_utils::default_configuration_for_tests);
    ASSERT_TRUE(cfg);
    auto default_obj = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    ASSERT_EQ(*default_obj, *cfg);
}

TEST_F(configuration_test, create_from_input_stream) {
    std::string content{
        "[datastore]\n"
        "log_location=LOCATION\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};
    auto section = cfg.get_section("datastore");
    ASSERT_TRUE(section);
    auto value = section->get<std::string>("log_location");
    ASSERT_TRUE(value);
    EXPECT_EQ("LOCATION", *value);
}

TEST_F(configuration_test, equality_of_objs_created_from_same_content) {
    std::string content{
        "[datastore]\n"
        "log_location=LOCATION\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};
    std::stringstream ss1{content};
    configuration::whole exp{ss1, tateyama::test_utils::default_configuration_for_tests};
    EXPECT_EQ(exp, cfg);
}

TEST_F(configuration_test, inequality_when_new_property_is_added) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    auto section = cfg->get_section("datastore"); // any section name is fine for testing
    ASSERT_TRUE(section);
    ASSERT_TRUE(section->set("test", "true"));

    auto orig = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    EXPECT_EQ(*orig, *cfg);
}

TEST_F(configuration_test, warn_on_invalid_property) {
    std::string content{
            "[sql]\n"
            "stealing_wait=2\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0};
}

TEST_F(configuration_test, empty_string) {
    std::string content{
        "[datastore]\n"
        "log_location=\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};
    auto section = cfg.get_section("datastore");
    ASSERT_TRUE(section);

    auto value = section->get<std::string>("log_location");
    ASSERT_TRUE(value);
    EXPECT_EQ("", *value);

    auto novalue = section->get<std::string>("log_loc");
    ASSERT_FALSE(novalue);
}

TEST_F(configuration_test, empty_number) {
    std::string content{
            "[sql]\n"
            "default_partitions=\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};

    auto section = cfg.get_section("sql");
    ASSERT_TRUE(section);

    auto value = section->get<std::size_t>("default_partitions");
    ASSERT_FALSE(value);
}

TEST_F(configuration_test, overflow_number) {
    std::string content{
            "[sql]\n"
            "default_partitions=1234567890123456789\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};

    auto section = cfg.get_section("sql");
    ASSERT_TRUE(section);

    try {
        auto value = section->get<std::int32_t>("default_partitions");
        FAIL();
    } catch (std::runtime_error &) {
        SUCCEED();
    }
}

TEST_F(configuration_test, abs_path) {
    std::string content{
            "[datastore]\n"
            "log_location=/tmp\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};

    auto section = cfg.get_section("datastore");
    ASSERT_TRUE(section);

    auto loc = section->get<std::filesystem::path>("log_location");
    EXPECT_TRUE(loc);
    EXPECT_EQ(loc.value(), "/tmp");
}

TEST_F(configuration_test, rel_path) {
    std::string content{
            "[datastore]\n"
            "log_location=relpath\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};
    std::filesystem::path bp = "/tmp";
    cfg.base_path(bp);

    auto section = cfg.get_section("datastore");
    ASSERT_TRUE(section);

    auto loc = section->get<std::filesystem::path>("log_location");
    EXPECT_TRUE(loc);
    EXPECT_EQ(loc.value(), "/tmp/relpath");
}

TEST_F(configuration_test, rel_path_base_empty) {
    std::string content{
            "[datastore]\n"
            "log_location=relpath\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};

    auto section = cfg.get_section("datastore");
    ASSERT_TRUE(section);

    try {
        auto loc = section->get<std::filesystem::path>("log_location");
        FAIL();
    } catch (std::runtime_error &) {
        SUCCEED();
    }
}

TEST_F(configuration_test, not_exist) {
    std::string content{
            "[sql]\n"
            "default_partitions=\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};

    auto section = cfg.get_section("sql");
    ASSERT_TRUE(section);

    auto value = section->get<bool>("property_not_exist");
    ASSERT_FALSE(value);
}

TEST_F(configuration_test, vector) {
    std::string content{
        "[authentication]\n"
          "administrators=root , admin , manager\n"
    };
    std::stringstream ss0{content};
    configuration::whole cfg{ss0, tateyama::test_utils::default_configuration_for_tests};

    auto section = cfg.get_section("authentication");
    ASSERT_TRUE(section);

    auto value = section->get<std::vector<std::string>>("administrators");
    ASSERT_TRUE(value);
    auto administrators = value.value();
    ASSERT_EQ(administrators.size(), 3);
    ASSERT_EQ(administrators.at(0), "root");
    ASSERT_EQ(administrators.at(1), "admin");
    ASSERT_EQ(administrators.at(2), "manager");
}

}
