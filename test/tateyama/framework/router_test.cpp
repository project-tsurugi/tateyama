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
#include <tateyama/framework/repository.h>

#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/task_scheduler_resource.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/api/server/request.h>
#include <tateyama/server/impl/request.h>
#include <tateyama/proto/test.pb.h>

#include <gtest/gtest.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;

class router_test : public ::testing::Test {

};

using namespace std::string_view_literals;

class test_service : public service {
public:
    static constexpr id_type tag = 0;
    test_service() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void operator()(std::shared_ptr<request>, std::shared_ptr<response>) override {}
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
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

    std::size_t session_id_{};
    std::size_t service_id_{};
    std::string payload_{};
};

TEST_F(router_test, basic) {
    auto cfg = api::configuration::create_configuration("");
    server sv{boot_mode::database_server, cfg};
    auto svc0 = std::make_shared<test_service>();
    sv.add_service(svc0);
    install_core_components(sv);

    auto router = sv.find_service<framework::routing_service>();
    ASSERT_TRUE(router);

    std::stringstream ss{};
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(999);
        ASSERT_TRUE(msg.SerializeToOstream(&ss));
    }
    auto str = ss.str();
    auto svrreq = std::make_shared<test_request>(10, 100, str);
//    auto svrreq = std::make_shared<api::server::response>(10, 100, str);
//
//    (*router)()
//    EXPECT_EQ(res0, sv.find_resource_by_id(0));
//    EXPECT_EQ(res1, sv.find_resource_by_id(1));
//    EXPECT_EQ(res0, sv.find_resource<test_resource0>());
//    EXPECT_EQ(res1, sv.find_resource<test_resource1>());
//
//    EXPECT_EQ(svc0, sv.find_service_by_id(0));
//    EXPECT_EQ(svc1, sv.find_service_by_id(1));
//    EXPECT_EQ(svc0, sv.find_service<test_service0>());
//    EXPECT_EQ(svc1, sv.find_service<test_service1>());

    sv.start();
    sv.shutdown();
}

}