/*
 * Copyright 2026-2026 Project Tsurugi.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <memory>
#include <sstream>
#include <stdexcept>
#include <gtest/gtest.h>
#include <grpcpp/impl/service_type.h>
#include <tateyama/api/configuration.h>
#include <tateyama/framework/environment.h>
#include <tateyama/grpc/server_resource.h>

namespace tateyama::grpc {

TEST(grpc_service_registration_test, preserves_ownership_until_shutdown_after_start) {
    std::stringstream ss{"[grpc_server]\nenabled=false\n"};
    auto cfg = std::make_shared<api::configuration::whole>(ss);
    framework::environment env{framework::boot_mode::database_server, cfg};
    grpc_server_resource listener{};
    ASSERT_TRUE(listener.setup(env));
    auto service = std::make_shared<::grpc::Service>();
    std::weak_ptr<::grpc::Service> original = service;
    listener.add_services("test", {service});
    service.reset();
    ASSERT_FALSE(original.expired());
    ASSERT_TRUE(listener.shutdown(env)); // failed setup must retain registrations
    ASSERT_FALSE(original.expired());
    ASSERT_TRUE(listener.setup(env));
    listener.add_services("test", {original.lock()}); // same registration is a no-op
    ASSERT_TRUE(listener.start(env));
    EXPECT_THROW(listener.add_services("other", {original.lock()}), std::runtime_error);
    ASSERT_TRUE(listener.shutdown(env));
    EXPECT_TRUE(original.expired());
    EXPECT_TRUE(listener.shutdown(env));
    EXPECT_FALSE(listener.start(env));
}

TEST(grpc_service_registration_test, rejects_conflicting_keys_and_duplicate_pointers) {
    grpc_server_resource listener{};
    auto first = std::make_shared<::grpc::Service>();
    auto other = std::make_shared<::grpc::Service>();
    listener.add_services("test", {first});
    EXPECT_NO_THROW(listener.add_services("test", {first}));
    EXPECT_THROW(listener.add_services("test", {other}), std::invalid_argument);
    auto different_owner = std::make_shared<int>(0);
    std::shared_ptr<::grpc::Service> same_pointer{different_owner, first.get()};
    EXPECT_THROW(listener.add_services("test", {same_pointer}), std::invalid_argument);
    EXPECT_THROW(listener.add_services("other", {first}), std::invalid_argument);
    EXPECT_THROW(listener.add_services("group", {other, first}), std::invalid_argument);
    EXPECT_THROW(listener.add_services("group", {other, other}), std::invalid_argument);
    EXPECT_NO_THROW(listener.add_services("group", {other}));
}

TEST(grpc_service_registration_test, rejects_empty_or_unowned_registrations) {
    grpc_server_resource listener{};
    auto service = std::make_shared<::grpc::Service>();
    EXPECT_THROW(listener.add_services("", {service}), std::invalid_argument);
    EXPECT_THROW(listener.add_services("test", {}), std::invalid_argument);
    EXPECT_THROW(listener.add_services("test", {nullptr}), std::invalid_argument);
    std::shared_ptr<::grpc::Service> unowned{std::shared_ptr<void>{}, service.get()};
    EXPECT_THROW(listener.add_services("test", {unowned}), std::invalid_argument);
    EXPECT_NO_THROW(listener.add_services("test", {service}));
}

TEST(grpc_service_registration_test, retains_aggregate_owner_for_aliased_service) {
    struct owner {
        ::grpc::Service service{};
    };
    std::weak_ptr<owner> original{};
    {
        grpc_server_resource listener{};
        auto aggregate = std::make_shared<owner>();
        original = aggregate;
        listener.add_services("aggregate", {std::shared_ptr<::grpc::Service>{aggregate, &aggregate->service}});
        aggregate.reset();
        EXPECT_FALSE(original.expired());
    }
    EXPECT_TRUE(original.expired());
}

} // namespace tateyama::grpc
