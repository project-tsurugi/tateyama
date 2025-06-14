/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include <tateyama/session/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::session {

class session_info_for_sessiopn_gc_test : public tateyama::endpoint::common::session_info_impl {
public:
    session_info_for_sessiopn_gc_test(std::size_t id, std::string_view con_type, std::string_view con_info)
        : session_info_impl(id, con_type, con_info) {
    }
    void set_appendix(const std::string& l, const std::string& a, const std::string& u) {
        label(l);
        application_name(a);
        user_name(u);
    }
};

class session_gc_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();
        session_context_ = std::make_shared<tateyama::session::resource::session_context_impl>(session_info_for_existing_session_, session_variable_set);
        session_info_for_existing_session_.set_appendix("label_fot_test", "application_for_test", "user_fot_test");
    }
    void TearDown() override {
        temporary_.clean();
    }

private:
    session_info_for_sessiopn_gc_test session_info_for_existing_session_{111, "IPC", "9999"};
    std::vector<std::tuple<std::string, tateyama::session::session_variable_set::variable_type, tateyama::session::session_variable_set::value_type>> variable_declarations_ {
        {"test_integer", tateyama::session::session_variable_type::signed_integer, static_cast<std::int64_t>(123)}
    };
    tateyama::session::session_variable_set session_variable_set{variable_declarations_};

protected:
    std::shared_ptr<tateyama::session::resource::session_context_impl> session_context_{};
};

TEST_F(session_gc_test, session_list) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();

    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto slrq = rq.mutable_session_list();
            str = rq.SerializeAsString();
            rq.clear_session_list();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionList slrs{};
        EXPECT_TRUE(slrs.ParseFromString(body));
        EXPECT_TRUE(slrs.has_success());
        auto success = slrs.success();
        EXPECT_EQ(0, success.entries_size());
    }

    ss->register_session(session_context_);
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto slrq = rq.mutable_session_list();
            str = rq.SerializeAsString();
            rq.clear_session_list();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionList slrs{};
        EXPECT_TRUE(slrs.ParseFromString(body));
        EXPECT_TRUE(slrs.has_success());
        auto success = slrs.success();
        EXPECT_EQ(1, success.entries_size());
    }

    session_context_ = nullptr;
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto slrq = rq.mutable_session_list();
            str = rq.SerializeAsString();
            rq.clear_session_list();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionList slrs{};
        EXPECT_TRUE(slrs.ParseFromString(body));
        EXPECT_TRUE(slrs.has_success());
        auto success = slrs.success();
        EXPECT_EQ(0, success.entries_size());
    }

    sv.shutdown();
}

}
