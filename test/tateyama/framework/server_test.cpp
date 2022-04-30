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

#include <gtest/gtest.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;

class server_test : public ::testing::Test {

};

using namespace std::string_view_literals;

class test_resource0 : public resource {
public:
    static constexpr id_type tag = 0;
    test_resource0() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
};

class test_resource1 : public resource {
public:
    static constexpr id_type tag = 1;
    test_resource1() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
};

class test_service0 : public service {
public:
    static constexpr id_type tag = 0;
    test_service0() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void operator()(std::shared_ptr<request>, std::shared_ptr<response>) override {}
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
};

class test_service1 : public service {
public:
    static constexpr id_type tag = 1;
    test_service1() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void operator()(std::shared_ptr<request>, std::shared_ptr<response>) override {}
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
};

TEST_F(server_test, basic) {
    auto cfg = api::configuration::create_configuration("");
    server sv{boot_mode::database_server, cfg};
    auto res0 = std::make_shared<test_resource0>();
    auto res1 = std::make_shared<test_resource1>();
    sv.add_resource(res1);
    sv.add_resource(res0);
    auto svc0 = std::make_shared<test_service0>();
    auto svc1 = std::make_shared<test_service1>();
    sv.add_service(svc0);
    sv.add_service(svc1);

    EXPECT_EQ(res0, sv.find_resource_by_id(0));
    EXPECT_EQ(res1, sv.find_resource_by_id(1));
    EXPECT_EQ(res0, sv.find_resource<test_resource0>());
    EXPECT_EQ(res1, sv.find_resource<test_resource1>());

    EXPECT_EQ(svc0, sv.find_service_by_id(0));
    EXPECT_EQ(svc1, sv.find_service_by_id(1));
    EXPECT_EQ(svc0, sv.find_service<test_service0>());
    EXPECT_EQ(svc1, sv.find_service<test_service1>());

    sv.start();
    sv.shutdown();
}

TEST_F(server_test, install_core_components) {
    auto cfg = api::configuration::create_configuration("");
    server sv{boot_mode::database_server, cfg};
    auto res0 = std::make_shared<test_resource0>();
    install_core_components(sv);

    auto router = sv.find_service<routing_service>();
    ASSERT_TRUE(router);
    auto kvs = std::static_pointer_cast<transactional_kvs_resource>(sv.find_resource<transactional_kvs_resource>());
    ASSERT_TRUE(kvs);
    kvs->handle();
    sv.start();
    sv.shutdown();
}

TEST_F(server_test, add_task_scheduler) {
    auto cfg = api::configuration::create_configuration("");
    server sv{boot_mode::database_server, cfg};
    struct task {};
    sv.add_resource(std::make_shared<task_scheduler_resource<task>>());
    auto sched = sv.find_resource<task_scheduler_resource<task>>();
    ASSERT_TRUE(sched);
}
}