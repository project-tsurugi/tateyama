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
#include <tateyama/datastore/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

#include "tateyama/status/resource/database_info_impl.h"
#include "tateyama/endpoint/common/session_info_impl.h"

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>
#include <tateyama/test_utils/request_response.h>

namespace tateyama::datastore {

using namespace std::literals::string_literals;

class datastore_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();
    }
    void TearDown() override {
        temporary_.clean();
    }
};

using namespace std::string_view_literals;

TEST_F(datastore_test, basic) {
    ::sharksfin::Slice name{};
    if (auto rc = ::sharksfin::implementation_id(&name); rc == ::sharksfin::StatusCode::OK && name == "memory") {
        GTEST_SKIP() << "tateyama-memory doesn't support datastore";
    }
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    add_core_components(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    std::string str{};
    {
        ::tateyama::proto::datastore::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto bb = rq.mutable_backup_begin();
        str = rq.SerializeAsString();
        rq.clear_backup_begin();
    }
    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, datastore::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;
    std::vector<std::string> files{};
    ::tateyama::proto::datastore::response::BackupBegin bb{};
    EXPECT_TRUE(bb.ParseFromString(body));
    EXPECT_TRUE(bb.has_success());
    EXPECT_EQ(3, bb.success().simple_source().files_size());
    for(auto&& f : bb.success().simple_source().files()) {
        std::cout << "backup file: " << f << std::endl;
    }
    sv.shutdown();
}

#if 0
TEST_F(datastore_test, test_connectivity_with_limestone) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    add_core_components(sv);
    sv.start();
    auto ds = sv.find_service<datastore::service::bridge>();
    datastore::



    sv.shutdown();
}
#endif

}
