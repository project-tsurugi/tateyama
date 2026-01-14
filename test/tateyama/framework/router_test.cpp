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
#include <tateyama/framework/routing_service.h>

#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/api/server/request.h>
#include <tateyama/proto/test.pb.h>
#include <tateyama/proto/core/request.pb.h>
#include <tateyama/proto/core/response.pb.h>

#include <tateyama/api/server/database_info.h>
#include "tateyama/endpoint/common/session_info_impl.h"

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>
#include <tateyama/test_utils/request_response.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class router_test :
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

    class test_service : public service {
    public:
        static constexpr id_type tag = max_system_reserved_id;
        test_service() = default;
        [[nodiscard]] id_type id() const noexcept override {
            return tag;
        }
        bool operator()(std::shared_ptr<request>, std::shared_ptr<response>) override {
            called_ = true;
            return true;
        }
        bool setup(environment&) override { return true;}
        bool start(environment&) override { return true;}
        bool shutdown(environment&) override { return true;}
        std::string_view label() const noexcept override { return "test_service"; }
        bool called_{false};
    };

    class test_response_for_the_test : public tateyama::test_utils::test_response {
    public:
        void error(proto::diagnostics::Record const& record) override {
            error_invoked_ = true;
            error_record_ = record;
        }

        bool error_invoked_{};
        proto::diagnostics::Record error_record_{};
    };

};

TEST_F(router_test, basic) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    server sv{boot_mode::database_server, cfg};
    auto svc0 = std::make_shared<test_service>();
    sv.add_service(svc0);
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();

    auto router = sv.find_service<framework::routing_service>();
    ASSERT_TRUE(router);
    ASSERT_EQ(framework::routing_service::tag, router->id());

    std::stringstream ss{};
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(999);
        ASSERT_TRUE(msg.SerializeToOstream(&ss));
    }
    auto str = ss.str();
    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, test_service::tag, str);
    auto svrres = std::make_shared<test_response_for_the_test>();

    ASSERT_FALSE(svc0->called_);
    (*router)(svrreq, svrres);
    ASSERT_TRUE(svc0->called_);
    sv.shutdown();
}

TEST_F(router_test, update_expiration_time) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    server sv{boot_mode::database_server, cfg};
    auto svc0 = std::make_shared<test_service>();
    sv.add_service(svc0);
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();

    auto router = sv.find_service<framework::routing_service>();
    ASSERT_TRUE(router);
    ASSERT_EQ(framework::routing_service::tag, router->id());

    std::stringstream ss{};
    {
        ::tateyama::proto::core::request::Request msg{};
        auto uxt = msg.mutable_update_expiration_time();
        uxt->set_expiration_time(100);
        ASSERT_TRUE(msg.SerializeToOstream(&ss));
    }
    auto str = ss.str();
    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, routing_service::tag, str);
    auto svrres = std::make_shared<test_response_for_the_test>();

    ASSERT_FALSE(svc0->called_);
    (*router)(svrreq, svrres);
    ASSERT_FALSE(svc0->called_);

    // update_expiration_time is now handled at the endpoint
    ASSERT_TRUE(svrres->error_invoked_);
    ASSERT_EQ(svrres->error_record_.code(), ::tateyama::proto::diagnostics::Code::UNSUPPORTED_OPERATION);

    sv.shutdown();
}

TEST_F(router_test, invalid_service_id) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    server sv{boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();

    auto router = sv.find_service<framework::routing_service>();
    ASSERT_TRUE(router);
    ASSERT_EQ(framework::routing_service::tag, router->id());

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, 999, "");  // 999 is an invalid service id
    auto svrres = std::make_shared<test_response_for_the_test>();

    (*router)(svrreq, svrres);

    ASSERT_TRUE(svrres->error_invoked_);
    ASSERT_EQ(svrres->error_record_.code(), ::tateyama::proto::diagnostics::Code::SERVICE_UNAVAILABLE);
    ASSERT_EQ(svrres->error_record_.message().find("unsupported service message: the destination service (ID=999) is not registered."), 0);
    sv.shutdown();
}
}
