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
#include <tateyama/altimeter/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/altimeter/request.pb.h>
#include <tateyama/proto/altimeter/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::altimeter {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class altimeter_helper_test : public tateyama::altimeter::service::altimeter_helper {
public:
    enum call_type {
        none = 0,
        set_enabled_call,
        set_level_call,
        set_stmt_duration_threshold_call,
        rotate_all_call
    };

    void set_enabled(std::string_view type, const bool& enabled) override {
        EXPECT_EQ(call_type_, call_type::none);
        call_type_ = set_enabled_call;
        log_type_ = type;
        enabled_ = enabled;
    }
    void set_level(std::string_view type, std::uint64_t level) override {
        EXPECT_EQ(call_type_, call_type::none);
        call_type_ = set_level_call;
        log_type_ = type;
        level_ = level;
    }
    void set_stmt_duration_threshold(std::uint64_t duration) override {
        EXPECT_EQ(call_type_, call_type::none);
        call_type_ = set_stmt_duration_threshold_call;
        duration_ = duration;
    };
    void rotate_all(std::string_view type) override {
        EXPECT_EQ(call_type_, call_type::none);
        call_type_ = rotate_all_call;
        log_type_ = type;
    };

    call_type call() { return call_type_; }
    std::string& log() { return log_type_; }
    std::uint64_t level() {return level_; };
    std::uint64_t duration() {return duration_; };
    bool enabled() {return enabled_; };

private:
    call_type call_type_{};
    std::string log_type_{};
    std::uint64_t level_{};
    std::uint64_t duration_{};
    bool enabled_{};
};

class altimeter_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();

        auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
        set_dbpath(*cfg);
        sv_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        tateyama::test_utils::add_core_components_for_test(*sv_);
        sv_->start();

        router_ = sv_->find_service<framework::routing_service>();
        ASSERT_TRUE(router_);
        EXPECT_EQ(framework::routing_service::tag, router_->id());

        bridge_ = sv_->find_service<altimeter::service::bridge>();
        ASSERT_TRUE(bridge_);
        EXPECT_EQ(tateyama::altimeter::service::bridge::tag, bridge_->id());

        helper_ = std::make_shared<altimeter_helper_test>();
        bridge_->replace_helper(helper_);
    }
    void TearDown() override {
        sv_->shutdown();

        temporary_.clean();
    }

private:
    std::unique_ptr<framework::server> sv_{};
    std::shared_ptr<tateyama::altimeter::service::bridge> bridge_{};

protected:
    std::shared_ptr<framework::service> router_{};
    std::shared_ptr<altimeter_helper_test> helper_{};
};


// enable
TEST_F(altimeter_test, enable_event) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_event_log()->set_enabled(true);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_enabled_call);
    EXPECT_EQ(helper_->log(), "event");
    EXPECT_EQ(helper_->enabled(), true);
}

TEST_F(altimeter_test, enable_audit) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_audit_log()->set_enabled(true);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_enabled_call);
    EXPECT_EQ(helper_->log(), "audit");
    EXPECT_EQ(helper_->enabled(), true);
}

// disable
TEST_F(altimeter_test, disable_event) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_event_log()->set_enabled(false);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_enabled_call);
    EXPECT_EQ(helper_->log(), "event");
    EXPECT_EQ(helper_->enabled(), false);
}

TEST_F(altimeter_test, disable_audit) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_audit_log()->set_enabled(false);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_enabled_call);
    EXPECT_EQ(helper_->log(), "audit");
    EXPECT_EQ(helper_->enabled(), false);
}

// set level
TEST_F(altimeter_test, level_event) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_event_log()->set_level(12);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_level_call);
    EXPECT_EQ(helper_->log(), "event");
    EXPECT_EQ(helper_->level(), 12);
}

TEST_F(altimeter_test, level_audit) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_audit_log()->set_level(12);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_level_call);
    EXPECT_EQ(helper_->log(), "audit");
    EXPECT_EQ(helper_->level(), 12);
}

// set statement_duration
TEST_F(altimeter_test, statement_duration) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_configure = rq.mutable_configure();
        mutable_configure->mutable_event_log()->set_statement_duration(12345);
        str = rq.SerializeAsString();
        rq.clear_configure();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_stmt_duration_threshold_call);
    EXPECT_EQ(helper_->duration(), 12345);
}

// rotate
TEST_F(altimeter_test, rotete_event) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_log_rotate = rq.mutable_log_rotate();
        mutable_log_rotate->set_category(::tateyama::proto::altimeter::common::LogCategory::EVENT);
        str = rq.SerializeAsString();
        rq.clear_log_rotate();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::rotate_all_call);
    EXPECT_EQ(helper_->log(), "event");
}

TEST_F(altimeter_test, rotete_audit) {
    std::string str{};
    {
        ::tateyama::proto::altimeter::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* mutable_log_rotate = rq.mutable_log_rotate();
        mutable_log_rotate->set_category(::tateyama::proto::altimeter::common::LogCategory::AUDIT);
        str = rq.SerializeAsString();
        rq.clear_log_rotate();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::rotate_all_call);
    EXPECT_EQ(helper_->log(), "audit");
}

}
