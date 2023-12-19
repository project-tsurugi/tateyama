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
#include <tateyama/framework/transactional_kvs_resource.h>

#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/api/server/request.h>
#include <tateyama/proto/test.pb.h>

#include <gtest/gtest.h>
#include <tateyama/utils/test_utils.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class transactional_kvs_test :
    public ::testing::Test,
    public test::test_utils {
public:
    void SetUp() override {
        temporary_.prepare();
    }
    void TearDown() override {
        temporary_.clean();
    }
};

TEST_F(transactional_kvs_test, basic) {
    std::stringstream ss{};
    ss << "[datastore]\n";
    ss << "log_location=";
    ss << path();
    ss << "\n";
    std::cerr << "ss : " << ss.str();
    auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test::default_configuration_for_tests);
    framework::environment env{boot_mode::database_server, cfg};
    transactional_kvs_resource kvs{};
    ASSERT_TRUE(kvs.setup(env));
    ASSERT_TRUE(kvs.start(env));
    ASSERT_TRUE(kvs.shutdown(env));
}

TEST_F(transactional_kvs_test, relative_path) {
    // verify relative path appended after base path
    std::stringstream ss{
        "[datastore]\n"
        "log_location=db\n",
    };
    auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test::default_configuration_for_tests);
    cfg->base_path(path());
    framework::environment env{boot_mode::database_server, cfg};
    transactional_kvs_resource kvs{};
    // we can only check following calls are successful
    // manually verify with GLOG_v=50 env. var. and shirakami::init receives db directory under tmp folder as log_directory_path
    ASSERT_TRUE(kvs.setup(env));
    ASSERT_TRUE(kvs.start(env));
    ASSERT_TRUE(kvs.shutdown(env));
}

TEST_F(transactional_kvs_test, empty_string) {
    // verify error with empty string
    std::stringstream ss{
        "[datastore]\n"
        "log_location=\n",
    };
    auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test::default_configuration_for_tests);
    cfg->base_path(path());
    framework::environment env{boot_mode::database_server, cfg};
    transactional_kvs_resource kvs{};
    ASSERT_FALSE(kvs.setup(env));
}

TEST_F(transactional_kvs_test, DISABLED_error_detection) {
    // rise error from datastore
    std::stringstream ss{
        "[datastore]\n"
        "log_location=/does_not_exist\n",  // to arise error
    };
    auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test::default_configuration_for_tests);
    cfg->base_path(path());
    framework::environment env{boot_mode::database_server, cfg};
    transactional_kvs_resource kvs{};
    // we can only check following calls are successful
    // manually verify with GLOG_v=50 env. var. and shirakami::init receives empty string as log_directory_path

    ASSERT_FALSE(kvs.setup(env));
}
}
