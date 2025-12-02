/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <filesystem>
#include <gtest/gtest.h>

#include <tateyama/api/server/request.h>
#include <tateyama/datastore/resource/bridge.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/proto/test.pb.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class transactional_kvs_test :
    public ::testing::Test,
    public test_utils::utility {
public:
    void SetUp() override {
        temporary_.prepare();
    }
    void TearDown() override {
        temporary_.clean();
    }
};

TEST_F(transactional_kvs_test, fail_without_registering_datastore_resource) {
    std::stringstream ss{};
    ss << "[datastore]\n";
    ss << "log_location=" << path() << "\n";
    auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test_utils::default_configuration_for_tests);
    framework::environment env{boot_mode::database_server, cfg};
    transactional_kvs_resource kvs{};
    ASSERT_FALSE(kvs.setup(env) && kvs.start(env));
}

TEST_F(transactional_kvs_test, basic) {
    bool using_shirakami = false;
    ::sharksfin::Slice name{};
    if (auto rc = ::sharksfin::implementation_id(&name); rc == ::sharksfin::StatusCode::OK && name == "shirakami") {
        using_shirakami = true;
    }
    std::stringstream ss{};
    ss << "[datastore]\n";
    ss << "log_location=" << path() << "\n";
    std::filesystem::path logdir{path()};
    auto cfg = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test_utils::default_configuration_for_tests);
    framework::environment env{boot_mode::database_server, cfg};
    auto ds = std::make_shared<datastore::resource::bridge>();
    env.resource_repository().add(ds);
    transactional_kvs_resource kvs{};
    ASSERT_FALSE(std::filesystem::exists(logdir) && !std::filesystem::is_empty(logdir)) << logdir;
    ASSERT_TRUE(ds->setup(env));
    ASSERT_TRUE(kvs.setup(env));
    ASSERT_TRUE(ds->start(env));
    ASSERT_TRUE(kvs.start(env));
    if (using_shirakami) {
        // TODO?: force creating pwal file, eg. CREATE TABLE
    }
    ASSERT_TRUE(kvs.shutdown(env));
    ASSERT_TRUE(ds->shutdown(env));
    // assume: limestone creates something in logdir
    ASSERT_TRUE(std::filesystem::exists(logdir) && !std::filesystem::is_empty(logdir)) << logdir;
    if (using_shirakami) {
        // TODO?: test pwal files
    }
}

}
