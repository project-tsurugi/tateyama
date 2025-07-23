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
#include <tateyama/session/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::session {

using namespace std::literals::string_literals;

class session_info_for_sessiopn_test : public tateyama::endpoint::common::session_info_impl {
public:
    session_info_for_sessiopn_test(std::size_t id, std::string_view con_type, std::string_view con_info)
        : session_info_impl(id, con_type, con_info, administrators_) {
    }
    void set_appendix(const std::string& l, const std::string& a, const std::string& u) {
        label(l);
        application_name(a);
        user_name(u);
    }

private:
    tateyama::endpoint::common::administrators administrators_{"*"};
};

class session_test :
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
    session_info_for_sessiopn_test session_info_for_existing_session_{111, "IPC", "9999"};
    std::vector<std::tuple<std::string, tateyama::session::session_variable_set::variable_type, tateyama::session::session_variable_set::value_type>> variable_declarations_ {
        {"test_integer", tateyama::session::session_variable_type::signed_integer, static_cast<std::int64_t>(123)},
        {"test_monostate", tateyama::session::session_variable_type::signed_integer, std::monostate{}},
    };
    tateyama::session::session_variable_set session_variable_set{variable_declarations_};

protected:
    std::shared_ptr<tateyama::session::resource::session_context_impl> session_context_{};
};

using namespace std::string_view_literals;

TEST_F(session_test, session_list) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);

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
    auto e = success.entries(0);
    EXPECT_EQ(":111", e.session_id());
    EXPECT_EQ("IPC", e.connection_type());
    EXPECT_EQ("9999", e.connection_info());
    EXPECT_EQ("label_fot_test", e.label());
    EXPECT_EQ("application_for_test", e.application());
    EXPECT_EQ("user_fot_test", e.user());

    sv.shutdown();
}

TEST_F(session_test, session_get_by_id) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);
    
    std::string str{};
    {
        ::tateyama::proto::session::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto sgrq = rq.mutable_session_get();
        sgrq->set_session_specifier(":111");
        str = rq.SerializeAsString();
        rq.clear_session_get();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::session::response::SessionGet sgrs{};
    EXPECT_TRUE(sgrs.ParseFromString(body));
    EXPECT_TRUE(sgrs.has_success());
    auto success = sgrs.success();
    auto e = success.entry();
    EXPECT_EQ(":111", e.session_id());
    EXPECT_EQ("IPC", e.connection_type());
    EXPECT_EQ("9999", e.connection_info());
    EXPECT_EQ("label_fot_test", e.label());
    EXPECT_EQ("application_for_test", e.application());
    EXPECT_EQ("user_fot_test", e.user());

    sv.shutdown();
}

TEST_F(session_test, session_get_by_label) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);
    
    std::string str{};
    {
        ::tateyama::proto::session::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto sgrq = rq.mutable_session_get();
        sgrq->set_session_specifier("label_fot_test");
        str = rq.SerializeAsString();
        rq.clear_session_get();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::session::response::SessionGet sgrs{};
    EXPECT_TRUE(sgrs.ParseFromString(body));
    EXPECT_TRUE(sgrs.has_success());
    auto success = sgrs.success();
    auto e = success.entry();
    EXPECT_EQ(":111", e.session_id());
    EXPECT_EQ("IPC", e.connection_type());
    EXPECT_EQ("9999", e.connection_info());
    EXPECT_EQ("label_fot_test", e.label());
    EXPECT_EQ("application_for_test", e.application());
    EXPECT_EQ("user_fot_test", e.user());

    sv.shutdown();
}

TEST_F(session_test, session_get_variable) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);
    
    std::string str{};
    {
        ::tateyama::proto::session::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto gvrq = rq.mutable_session_get_variable();
        gvrq->set_session_specifier(":111");
        gvrq->set_name("test_integer");
        str = rq.SerializeAsString();
        rq.clear_session_get_variable();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::session::response::SessionGetVariable gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_success());
    auto success = gvrs.success();
    EXPECT_EQ("test_integer", success.name());
    EXPECT_EQ(success.value_case(), ::tateyama::proto::session::response::SessionGetVariable_Success::ValueCase::kSignedIntegerValue);
    EXPECT_EQ(success.signed_integer_value(), 123);

    sv.shutdown();
}

TEST_F(session_test, session_set_variable) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);

    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto svrq = rq.mutable_session_set_variable();
            svrq->set_session_specifier(":111");
            svrq->set_name("test_integer");
            svrq->set_value("321");
            str = rq.SerializeAsString();
            rq.clear_session_set_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionSetVariable svrs{};
        EXPECT_TRUE(svrs.ParseFromString(body));
        EXPECT_TRUE(svrs.has_success());
    }

    {  // do get variable to check the result
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto gvrq = rq.mutable_session_get_variable();
            gvrq->set_session_specifier(":111");
            gvrq->set_name("test_integer");
            str = rq.SerializeAsString();
            rq.clear_session_get_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionGetVariable gvrs{};
        EXPECT_TRUE(gvrs.ParseFromString(body));
        EXPECT_TRUE(gvrs.has_success());
        auto success = gvrs.success();
        EXPECT_EQ("test_integer", success.name());
        EXPECT_EQ(success.value_case(), ::tateyama::proto::session::response::SessionGetVariable_Success::ValueCase::kSignedIntegerValue);
        EXPECT_EQ(success.signed_integer_value(), 321);
    }

    sv.shutdown();
}

TEST_F(session_test, session_unset_variable_monostate) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);

    // set variable
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto svrq = rq.mutable_session_set_variable();
            svrq->set_session_specifier(":111");
            svrq->set_name("test_monostate");
            svrq->set_value("321");
            str = rq.SerializeAsString();
            rq.clear_session_set_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionSetVariable svrs{};
        EXPECT_TRUE(svrs.ParseFromString(body));
        EXPECT_TRUE(svrs.has_success());
    }

    // unset variable
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto svrq = rq.mutable_session_set_variable();
            svrq->set_session_specifier(":111");
            svrq->set_name("test_monostate");
            str = rq.SerializeAsString();
            rq.clear_session_set_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionSetVariable svrs{};
        EXPECT_TRUE(svrs.ParseFromString(body));
        EXPECT_TRUE(svrs.has_success());
    }

    {  // do get variable to check the result
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto gvrq = rq.mutable_session_get_variable();
            gvrq->set_session_specifier(":111");
            gvrq->set_name("test_monostate");
            str = rq.SerializeAsString();
            rq.clear_session_get_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionGetVariable gvrs{};
        EXPECT_TRUE(gvrs.ParseFromString(body));
        EXPECT_TRUE(gvrs.has_success());
        EXPECT_EQ(gvrs.success().value_case(), ::tateyama::proto::session::response::SessionGetVariable_Success::ValueCase::VALUE_NOT_SET);
    }

    sv.shutdown();
}

TEST_F(session_test, session_unset_variable_value) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);

    // set variable
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto svrq = rq.mutable_session_set_variable();
            svrq->set_session_specifier(":111");
            svrq->set_name("test_monostate");
            svrq->set_value("321");
            str = rq.SerializeAsString();
            rq.clear_session_set_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionSetVariable svrs{};
        EXPECT_TRUE(svrs.ParseFromString(body));
        EXPECT_TRUE(svrs.has_success());
    }

    // unset variable
    {
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto svrq = rq.mutable_session_set_variable();
            svrq->set_session_specifier(":111");
            svrq->set_name("test_monostate");
            str = rq.SerializeAsString();
            rq.clear_session_set_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionSetVariable svrs{};
        EXPECT_TRUE(svrs.ParseFromString(body));
        EXPECT_TRUE(svrs.has_success());
    }

    {  // do get variable to check the result
        std::string str{};
        {
            ::tateyama::proto::session::request::Request rq{};
            rq.set_service_message_version_major(1);
            rq.set_service_message_version_minor(0);
            auto gvrq = rq.mutable_session_get_variable();
            gvrq->set_session_specifier(":111");
            gvrq->set_name("test_integer");
            str = rq.SerializeAsString();
            rq.clear_session_get_variable();
        }

        auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
        auto svrres = std::make_shared<tateyama::test_utils::test_response>();

        (*router)(svrreq, svrres);
        EXPECT_EQ(10, svrres->session_id_);
        auto& body = svrres->body_;

        ::tateyama::proto::session::response::SessionGetVariable gvrs{};
        EXPECT_TRUE(gvrs.ParseFromString(body));
        EXPECT_TRUE(gvrs.has_success());
        EXPECT_EQ(gvrs.success().value_case(), ::tateyama::proto::session::response::SessionGetVariable_Success::ValueCase::kSignedIntegerValue);
        EXPECT_EQ(gvrs.success().signed_integer_value(), static_cast<std::int64_t>(123));
    }

    sv.shutdown();
}

TEST_F(session_test, session_shutdown) {
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    framework::server sv{framework::boot_mode::database_server, cfg};
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    EXPECT_TRUE(router);
    EXPECT_EQ(framework::routing_service::tag, router->id());

    auto ss = sv.find_resource<session::resource::bridge>();
    ss->register_session(session_context_);
    auto context = ss->sessions_core().sessions().find_session(111);
    ASSERT_TRUE(context);
    EXPECT_EQ(tateyama::session::shutdown_request_type::nothing, context->shutdown_request());
    
    std::string str{};
    {
        ::tateyama::proto::session::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto shrq = rq.mutable_session_shutdown();
        shrq->set_session_specifier(":111");
        shrq->set_request_type(::tateyama::proto::session::request::SessionShutdownType::GRACEFUL);
        str = rq.SerializeAsString();
        rq.clear_session_shutdown();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, session::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->wait_and_get_body();
    EXPECT_EQ(tateyama::session::shutdown_request_type::graceful, context->shutdown_request());

    ::tateyama::proto::session::response::SessionShutdown shrs{};
    EXPECT_TRUE(shrs.ParseFromString(body));
    EXPECT_TRUE(shrs.has_success());

    sv.shutdown();
}

}
