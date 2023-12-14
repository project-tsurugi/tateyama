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

#include "tateyama/status/resource/database_info_impl.h"
#include "tateyama/endpoint//common/session_info_impl.h"

#include <gtest/gtest.h>
#include <tateyama/utils/test_utils.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class router_test :
    public ::testing::Test,
    public test::test_utils
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

    class test_request : public api::server::request {
    public:
        test_request(
            std::size_t session_id,
            std::size_t service_id,
            std::string_view payload
        ) :
            session_id_(session_id),
            service_id_(service_id),
            payload_(payload)
        {}

        [[nodiscard]] std::size_t session_id() const override {
            return session_id_;
        }

        [[nodiscard]] std::size_t service_id() const override {
            return service_id_;
        }

        [[nodiscard]] std::string_view payload() const override {
            return payload_;
        }

        tateyama::api::server::database_info const& database_info() const noexcept override {
            return database_info_;
        }

        tateyama::api::server::session_info const& session_info() const noexcept override {
            return session_info_;
        }

        std::size_t session_id_{};
        std::size_t service_id_{};
        std::string payload_{};

        tateyama::status_info::resource::database_info_impl database_info_{};
        tateyama::endpoint::common::session_info_impl session_info_{};
    };

    class test_response : public api::server::response {
    public:
        test_response() = default;

        void session_id(std::size_t id) override {
            session_id_ = id;
        };
        status body_head(std::string_view body_head) override { return status::ok; }
        status body(std::string_view body) override { body_ = body; return status::ok; }
        void error(proto::diagnostics::Record const& record) override {}
        status acquire_channel(std::string_view name, std::shared_ptr<api::server::data_channel>& ch) override { return status::ok; }
        status release_channel(api::server::data_channel& ch) override { return status::ok; }

        std::size_t session_id_{};
        std::string body_{};
    };

};

TEST_F(router_test, basic) {
    auto cfg = api::configuration::create_configuration("", tateyama::test::default_configuration_for_tests);
    set_dbpath(*cfg);
    server sv{boot_mode::database_server, cfg};
    auto svc0 = std::make_shared<test_service>();
    sv.add_service(svc0);
    add_core_components(sv);
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
    auto svrreq = std::make_shared<test_request>(10, test_service::tag, str);
    auto svrres = std::make_shared<test_response>();

    ASSERT_FALSE(svc0->called_);
    (*router)(svrreq, svrres);
    ASSERT_TRUE(svc0->called_);
    sv.shutdown();
}

TEST_F(router_test, update_expiration_time) {
    auto cfg = api::configuration::create_configuration("", tateyama::test::default_configuration_for_tests);
    set_dbpath(*cfg);
    server sv{boot_mode::database_server, cfg};
    auto svc0 = std::make_shared<test_service>();
    sv.add_service(svc0);
    add_core_components(sv);
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
    auto svrreq = std::make_shared<test_request>(10, routing_service::tag, str);
    auto svrres = std::make_shared<test_response>();

    ASSERT_FALSE(svc0->called_);
    (*router)(svrreq, svrres);
    ASSERT_FALSE(svc0->called_);

    auto pl = svrreq->payload();
    ::tateyama::proto::core::response::UpdateExpirationTime out{};
    ASSERT_TRUE(out.ParseFromArray(pl.data(), pl.size()));
    EXPECT_TRUE(out.has_success());
    sv.shutdown();
}
}
