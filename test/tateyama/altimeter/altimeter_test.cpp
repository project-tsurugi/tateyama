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
#include "altimeter_test_common.h"

#include <tateyama/altimeter/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/altimeter/request.pb.h>
#include <tateyama/proto/altimeter/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/utils/test_utils.h>

namespace tateyama::altimeter {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class altimeter_helper_test : public tateyama::altimeter::service::altimeter_helper {
public:
    enum call_type {
        none = 0,
        enable_call,
        disable_call,
        set_level_call,
        set_stmt_duration_threshold_call,
        rotate_all_call
    };

    void enable(std::string_view type) override {
        call_type_ = enable_call;
        log_type_ = type;
    }
    void disable(std::string_view type) override {
        call_type_ = disable_call;
        log_type_ = type;
    }
    void set_level(std::string_view type, std::uint64_t level) override {
        call_type_ = set_level_call;
        log_type_ = type;
        level_ = level;
    }
    void set_stmt_duration_threshold(std::uint64_t duration) override {
        call_type_ = set_stmt_duration_threshold_call;
        duration_ = duration;
    };
    void rotate_all(std::string_view type) override {
        call_type_ = rotate_all_call;
        log_type_ = type;
    };

    call_type call() { return call_type_; }
    std::string& log() { return log_type_; }
    std::uint64_t level() {return level_; };
    std::uint64_t duration() {return duration_; };

private:
    call_type call_type_{};
    std::string log_type_{};
    std::uint64_t level_{};
    std::uint64_t duration_{};
};

class altimeter_test :
    public ::testing::Test,
    public test::test_utils
{
public:
    void SetUp() override {
        temporary_.prepare();

        auto cfg = api::configuration::create_configuration("", tateyama::test::default_configuration_for_tests);
        set_dbpath(*cfg);
        sv_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        add_core_components(*sv_);
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

    auto svrreq = std::make_shared<test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::enable_call);
    EXPECT_EQ(helper_->log(), "event");
}

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

    auto svrreq = std::make_shared<test_request>(11, altimeter::service::bridge::tag, str);
    auto svrres = std::make_shared<test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);

    EXPECT_EQ(helper_->call(), altimeter_helper_test::call_type::set_level_call);
    EXPECT_EQ(helper_->level(), 12);
}

}
