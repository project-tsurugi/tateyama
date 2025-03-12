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
#include <vector>
#include <string>
#include <chrono>

#include <tateyama/request/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/endpoint/common/listener_common.h>

#include <tateyama/proto/request/request.pb.h>
#include <tateyama/proto/request/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::request {

class listener_fot_test : public tateyama::endpoint::common::listener_common {
public:
    void operator()() {}
    void arrive_and_wait() {}
    void terminate() {}
    void print_diagnostic(std::ostream&) {}
    void foreach_request(const callback& cb) {
        for (auto&& e : data_) {
            cb(std::make_shared<tateyama::test_utils::test_request>(std::get<0>(e), std::get<1>(e), std::get<2>(e), std::get<3>(e)), std::chrono::system_clock::now());
            std::this_thread::sleep_for( std::chrono::milliseconds(10) );
        }
    }
private:
    std::vector<std::tuple<std::size_t, std::size_t, std::size_t, std::string>> data_ = {
        {123, 3, 101, "abcdef"},
        {234, 4, 102, "defghi"},
        {345, 3, 103, "fghijk"},
        {456, 3, 104, "ijklm"},
        {567, 3, 105, "lmnop"},
    };
};

class request_bridge_test :
        public ::testing::Test,
        public test_utils::utility {
public:
    void SetUp() override {
        temporary_.prepare();
        auto cfg = tateyama::api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
        set_dbpath(*cfg);
        sv_ = std::make_unique<framework::server>(tateyama::framework::boot_mode::database_server, cfg);
        tateyama::test_utils::add_core_components_for_test(*sv_);
        sv_->start();
        router_ = sv_->find_service<framework::routing_service>();

        auto bridge = sv_->find_service<request::service::bridge>();
        bridge->register_endpoint_listener(std::make_shared<listener_fot_test>());
    }
    void TearDown() override {
        sv_->shutdown();
        temporary_.clean();
    }

protected:
    std::unique_ptr<framework::server> sv_{};
    std::shared_ptr<framework::routing_service> router_{};
};

static inline std::size_t unix_now() {
    auto tp = std::chrono::system_clock::now();
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) - std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);
    return secs.time_since_epoch().count() * 1000 + (ns.count() + 500000) / 1000000;
}

TEST_F(request_bridge_test, list_request) {
    std::string str{};
    {
        ::tateyama::proto::request::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* gvrq = rq.mutable_list_request();
        str = rq.SerializeAsString();
        rq.clear_list_request();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, request::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::request::response::ListRequest gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_success());
    auto& success = gvrs.success();
    EXPECT_EQ(5, success.requests_size());
    auto un = unix_now();
    for (auto&& e : success.requests()) {
        switch (e.session_id()) {
        case 123:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(101, e.request_id());
            EXPECT_EQ(strlen("abcdef"), e.payload_size());
            break;
        case 234:
            EXPECT_EQ(4, e.service_id());
            EXPECT_EQ(102, e.request_id());
            EXPECT_EQ(strlen("defghi"), e.payload_size());
            break;
        case 345:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(103, e.request_id());
            EXPECT_EQ(strlen("fghijk"), e.payload_size());
            break;
        case 456:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(104, e.request_id());
            EXPECT_EQ(strlen("ijklm"), e.payload_size());
            break;
        case 567:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(105, e.request_id());
            EXPECT_EQ(strlen("lmnop"), e.payload_size());
            break;
        default:
            FAIL();
        }
        EXPECT_TRUE(5000 > (un - e.started_time()));
    }
}

TEST_F(request_bridge_test, list_request_t3) {
    std::string str{};
    {
        ::tateyama::proto::request::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* gvrq = rq.mutable_list_request();
        gvrq->set_top(3);
        str = rq.SerializeAsString();
        rq.clear_list_request();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, request::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::request::response::ListRequest gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_success());
    auto& success = gvrs.success();
    EXPECT_EQ(3, success.requests_size());
    auto un = unix_now();
    for (auto&& e : success.requests()) {
        switch (e.session_id()) {
        case 123:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(101, e.request_id());
            EXPECT_EQ(strlen("abcdef"), e.payload_size());
            break;
        case 234:
            EXPECT_EQ(4, e.service_id());
            EXPECT_EQ(102, e.request_id());
            EXPECT_EQ(strlen("defghi"), e.payload_size());
            break;
        case 345:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(103, e.request_id());
            EXPECT_EQ(strlen("fghijk"), e.payload_size());
            break;
        default:
            FAIL();
        }
        EXPECT_TRUE(5000 > (un - e.started_time()));
    }
}

TEST_F(request_bridge_test, list_request_f3) {
    std::string str{};
    {
        ::tateyama::proto::request::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* gvrq = rq.mutable_list_request();
        gvrq->set_service_id(3);
        str = rq.SerializeAsString();
        rq.clear_list_request();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, request::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::request::response::ListRequest gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_success());
    auto& success = gvrs.success();
    EXPECT_EQ(4, success.requests_size());
    auto un = unix_now();
    for (auto&& e : success.requests()) {
        switch (e.session_id()) {
        case 123:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(101, e.request_id());
            EXPECT_EQ(strlen("abcdef"), e.payload_size());
            break;
        case 345:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(103, e.request_id());
            EXPECT_EQ(strlen("fghijk"), e.payload_size());
            break;
        case 456:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(104, e.request_id());
            EXPECT_EQ(strlen("ijklm"), e.payload_size());
            break;
        case 567:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(105, e.request_id());
            EXPECT_EQ(strlen("lmnop"), e.payload_size());
            break;
        default:
            FAIL();
        }
        EXPECT_TRUE(5000 > (un - e.started_time()));
    }
}

TEST_F(request_bridge_test, list_request_t3_f3) {
    std::string str{};
    {
        ::tateyama::proto::request::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* gvrq = rq.mutable_list_request();
        gvrq->set_service_id(3);
        gvrq->set_top(3);
        str = rq.SerializeAsString();
        rq.clear_list_request();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(10, request::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::request::response::ListRequest gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_success());
    auto& success = gvrs.success();
    EXPECT_EQ(3, success.requests_size());
    auto un = unix_now();
    for (auto&& e : success.requests()) {
        switch (e.session_id()) {
        case 123:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(101, e.request_id());
            EXPECT_EQ(strlen("abcdef"), e.payload_size());
            break;
        case 345:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(103, e.request_id());
            EXPECT_EQ(strlen("fghijk"), e.payload_size());
            break;
        case 456:
            EXPECT_EQ(3, e.service_id());
            EXPECT_EQ(104, e.request_id());
            EXPECT_EQ(strlen("ijklm"), e.payload_size());
            break;
        default:
            FAIL();
        }
        EXPECT_TRUE(5000 > (un - e.started_time()));
    }
}

TEST_F(request_bridge_test, get_payload) {
    std::string str{};
    {
        ::tateyama::proto::request::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* gvrq = rq.mutable_get_payload();
        gvrq->set_session_id(123);
        gvrq->set_request_id(101);
        str = rq.SerializeAsString();
        rq.clear_get_payload();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, request::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::request::response::GetPayload gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_success());
    auto& success = gvrs.success();
    EXPECT_EQ("abcdef", success.data());
}

TEST_F(request_bridge_test, get_payload_fail) {
    std::string str{};
    {
        ::tateyama::proto::request::request::Request rq{};
        rq.set_service_message_version_major(1);
        rq.set_service_message_version_minor(0);
        auto* gvrq = rq.mutable_get_payload();
        gvrq->set_session_id(999);  // not found
        gvrq->set_request_id(999);  // not found
        str = rq.SerializeAsString();
        rq.clear_get_payload();
    }

    auto svrreq = std::make_shared<tateyama::test_utils::test_request>(11, request::service::bridge::tag, str);
    auto svrres = std::make_shared<tateyama::test_utils::test_response>();

    (*router_)(svrreq, svrres);
    EXPECT_EQ(11, svrres->session_id_);
    auto& body = svrres->body_;

    ::tateyama::proto::request::response::GetPayload gvrs{};
    EXPECT_TRUE(gvrs.ParseFromString(body));
    EXPECT_TRUE(gvrs.has_error());
    auto& error = gvrs.error();
    EXPECT_EQ(tateyama::proto::request::diagnostics::Code::REQUEST_MISSING, error.code());
}

}
