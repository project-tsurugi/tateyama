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

namespace tateyama::configuration {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class configuration_provider_test : public ::testing::Test {
public:
    void SetUp() override {
        std::stringstream ss{
            "[ipc_endpoint]\n"
            "  database_name=database_name_for_test\n"
            "[system]\n"
            "  instance_id=instance-id-for-test\n"
        };
        auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test_utils::default_configuration_for_tests);
        server_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        tateyama::test_utils::add_core_components_for_test(*server_);
        server_->setup();
        server_->start();
    }
    void TearDown() override {
        server_->shutdown();
    }
protected:
    std::unique_ptr<framework::server> server_{};
};

TEST_F(configuration_provider_test, basic) {
    auto configuration_provider = server_->find_resource<configuration::configuration_provider>();
    EXPECT_NE(nullptr, configuration_provider.get());

    auto& dbinfo = configuration_provider->database_info();
    EXPECT_EQ("database_name_for_test", dbinfo.name());
    EXPECT_EQ("instance-id-for-test", dbinfo.instance_id());
}

}
