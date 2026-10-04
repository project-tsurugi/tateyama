/*
 * Copyright 2026-2026 Project Tsurugi.
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

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include <data_relay_grpc/proto/blob_relay/blob_relay_local.grpc.pb.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/grpc/blob_relay/service_adapter.h>
#include <tateyama/grpc/server_resource.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::grpc::blob_relay {

// Added after core resources so the relay is registered before setup fails.
class fail_once_relay_resource final : public framework::resource {
public:
    bool setup(framework::environment&) override { return attempts_++ > 0; }
    bool start(framework::environment&) override { return true; }
    bool shutdown(framework::environment&) override { return true; }
    [[nodiscard]] id_type id() const noexcept override { return max_system_reserved_id + 1; }
    [[nodiscard]] std::string_view label() const noexcept override { return "fail_once_relay_resource"; }
private:
    unsigned attempts_{};
};

class blob_relay_setup_retry_test : public ::testing::Test, public test_utils::utility {
public:
    void SetUp() override {
        temporary_.prepare();
        std::stringstream ss{};
        ss << "[grpc_server]\nenabled=true\nlisten_address=localhost:62348\n"
           << "[blob_relay]\nenabled=true\nsession_store=" << path() << "/session_store\n";
        auto cfg = std::make_shared<api::configuration::whole>(ss, test_utils::default_configuration_for_tests);
        set_dbpath(*cfg);
        server_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        framework::add_core_components(*server_);
    }
    void TearDown() override {
        if (server_) { server_->shutdown(); }
        temporary_.clean();
    }
protected:
    void check_rpc() {
        namespace proto = data_relay_grpc::proto::blob_relay::blob_relay_local;
        auto channel = ::grpc::CreateChannel("localhost:62348", ::grpc::InsecureChannelCredentials());
        auto stub = proto::BlobRelayLocal::NewStub(channel);
        proto::GetLocalRequest request{};
        request.set_api_version(std::numeric_limits<std::uint64_t>::max());
        proto::GetLocalResponse response{};
        ::grpc::ClientContext context{};
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{5});
        auto status = stub->Get(&context, request, &response);
        // This application-level rejection proves the registered RPC ran.
        EXPECT_EQ(::grpc::StatusCode::UNAVAILABLE, status.error_code());
        EXPECT_NE(std::string::npos, status.error_message().find("the requested API version"));
    }
    std::unique_ptr<framework::server> server_{};
};

TEST_F(blob_relay_setup_retry_test, normal_start_serves_rpc) {
    ASSERT_TRUE(server_->setup());
    auto relay = server_->find_resource<service_adapter>()->blob_relay_service();
    std::vector<std::shared_ptr<::grpc::Service>> services{};
    for (auto* service : relay->services()) { services.emplace_back(relay, service); }
    // Re-registering the same owned group must not duplicate RPC methods.
    server_->find_resource<grpc_server_resource>()->add_services("blob_relay", std::move(services));
    ASSERT_TRUE(server_->start());
    check_rpc();
}

TEST_F(blob_relay_setup_retry_test, later_setup_failure_preserves_service_on_retry) {
    server_->add_resource(std::make_shared<fail_once_relay_resource>());
    ASSERT_FALSE(server_->setup());
    auto adapter = server_->find_resource<service_adapter>();
    ASSERT_TRUE(adapter);
    std::weak_ptr<data_relay_grpc::blob_relay::blob_relay_service> original = adapter->blob_relay_service();
    ASSERT_FALSE(original.expired());
    ASSERT_TRUE(server_->setup());
    ASSERT_FALSE(original.expired());
    ASSERT_TRUE(server_->start());
    EXPECT_EQ(original.lock(), adapter->blob_relay_service());
    check_rpc();
    auto& session = adapter->blob_relay_service()->create_session();
    session.dispose();
}

} // namespace tateyama::grpc::blob_relay
