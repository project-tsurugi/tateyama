/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <tateyama/framework/server.h>
#include <tateyama/configuration/configuration_provider.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::api {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class configuration_vector_test : public ::testing::Test {
public:
    void SetUp() override {
        std::stringstream ss{
            "[ipc_endpoint]\n"
            "  database_name=database_name_for_test1,database_name_for_test2\n"
            "  allow_blob_privileged=true,false\n"
            "[blob_relay]\n"
            "  session_store=deamon_path_1|deamon_path_2\n"
            "  sstream_chunk_size=11|22\n"
        };
        cfg_ = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test_utils::default_configuration_for_tests);
    }
    void TearDown() override {
    }
protected:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
};

TEST_F(configuration_vector_test, string) {
    auto* ipc_endpoint_section = cfg_->get_section("ipc_endpoint");
    EXPECT_NE(nullptr, ipc_endpoint_section);

    auto ov = ipc_endpoint_section->get<std::string>("database_name", ",");
    EXPECT_TRUE(ov);
    EXPECT_EQ(2, ov.value().size());
    EXPECT_EQ("database_name_for_test1", ov.value().at(0));
    EXPECT_EQ("database_name_for_test2", ov.value().at(1));
}

TEST_F(configuration_vector_test, bool) {
    auto* ipc_endpoint_section = cfg_->get_section("ipc_endpoint");
    EXPECT_NE(nullptr, ipc_endpoint_section);

    auto ov = ipc_endpoint_section->get<bool>("allow_blob_privileged", ",");
    EXPECT_TRUE(ov);
    EXPECT_EQ(2, ov.value().size());
    EXPECT_EQ(true, ov.value().at(0));
    EXPECT_EQ(false, ov.value().at(1));
}

TEST_F(configuration_vector_test, blob_relay) {
    auto* blob_relay_section = cfg_->get_section("blob_relay");
    ASSERT_NE(nullptr, blob_relay_section);

    auto pv = blob_relay_section->get<std::string>("session_store", "|");
    EXPECT_TRUE(pv);
    EXPECT_EQ(2, pv.value().size());
    EXPECT_EQ("deamon_path_1", pv.value().at(0));
    EXPECT_EQ("deamon_path_2", pv.value().at(1));

    auto sv = blob_relay_section->get<std::uint32_t>("sstream_chunk_size", "|");
    EXPECT_TRUE(sv);
    EXPECT_EQ(2, sv.value().size());
    EXPECT_EQ(11, sv.value().at(0));
    EXPECT_EQ(22, sv.value().at(1));
}

}
