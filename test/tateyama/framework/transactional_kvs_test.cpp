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

class transactional_kvs_test : public ::testing::Test {
public:
};

TEST_F(transactional_kvs_test, basic) {
    auto cfg = api::configuration::create_configuration("", tateyama::test::default_configuration_for_tests);
    framework::environment env{boot_mode::database_server, cfg};
    transactional_kvs_resource kvs{};
    ASSERT_TRUE(kvs.setup(env));
    ASSERT_TRUE(kvs.start(env));
    ASSERT_TRUE(kvs.shutdown(env));
}

}
