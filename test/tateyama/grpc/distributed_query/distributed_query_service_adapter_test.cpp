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

#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

#include <tateyama/framework/server.h>
#include <tateyama/grpc/distributed_query/service_adapter.h>
#include <tateyama/proto/distributed_query/hello.grpc.pb.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::grpc::distributed_query {

namespace {

class test_remote_process_handler final : public remote_process_handler {
public:
    explicit test_remote_process_handler(
        bool success = true,
        remote_process_error_kind error_kind = remote_process_error_kind::invalid_request)
        : success_(success), error_kind_(error_kind) {}

    remote_process_result operator()(std::string_view payload) override {
        received_ = payload;
        if (!success_) return {false, {}, "rejected payload", error_kind_};
        return {true, "handled:" + std::string{payload}, {}};
    }

    [[nodiscard]] std::string const& received() const noexcept {
        return received_;
    }

private:
    bool success_{};
    remote_process_error_kind error_kind_{};
    std::string received_{};
};

class cancellation_test_remote_process_handler final : public remote_process_handler {
public:
    remote_process_result operator()(std::string_view) override {
        return {false, {}, "cancellation check was not provided", remote_process_error_kind::internal};
    }

    remote_process_result execute(
        std::string_view,
        cancellation_check const& is_cancelled) override
    {
        using namespace std::chrono_literals;
        for (std::size_t i = 0; i < 2000; ++i) {
            if (is_cancelled()) {
                cancellation_observed_.store(true);
                return {false, {}, "cancelled", remote_process_error_kind::cancelled};
            }
            std::this_thread::sleep_for(1ms);
        }
        return {false, {}, "cancellation was not observed", remote_process_error_kind::internal};
    }

    [[nodiscard]] bool cancellation_observed() const noexcept {
        return cancellation_observed_.load();
    }

private:
    std::atomic_bool cancellation_observed_{};
};

::grpc::Status execute_remote_process(
    proto::distributed_query::DistributedQuery::Stub& stub,
    ::grpc::ClientContext& context,
    proto::distributed_query::ExecuteRemoteProcessRequest const& request,
    proto::distributed_query::ExecuteRemoteProcessResponse& response)
{
    auto reader = stub.ExecuteRemoteProcess(&context, request);
    while (reader->Read(&response)) {
    }
    return reader->Finish();
}

} // namespace

class distributed_query_service_adapter_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();

        std::stringstream ss{};
        ss << "[grpc_server]\n";
        ss << "enabled=true\n";
        ss << "listen_address=" << endpoint << '\n';
        ss << "[datastore]\n";
        ss << "log_location=" << path() << '\n';
        auto cfg = std::make_shared<api::configuration::whole>(
            ss,
            test_utils::default_configuration_for_tests);
        server_ = std::make_unique<framework::server>(
            framework::boot_mode::database_server,
            cfg);
        framework::add_core_components(*server_);

        ASSERT_TRUE(server_->setup());
        ASSERT_TRUE(server_->start());
    }

    void TearDown() override {
        if (server_) {
            server_->shutdown();
        }
        temporary_.clean();
    }

protected:
    static void set_valid_metadata(proto::distributed_query::ExecuteRemoteProcessRequest& request) {
        auto* metadata = request.mutable_metadata();
        metadata->mutable_protocol_version()->set_major(1);
        metadata->mutable_protocol_version()->set_minor(0);
        metadata->set_coordinator_job_id("test-job");
        metadata->set_remote_execution_id("test-execution");
        metadata->set_attempt_number(1);
    }

    static constexpr std::string_view endpoint{"localhost:62346"};
    std::unique_ptr<framework::server> server_{};
};

TEST_F(distributed_query_service_adapter_test, hello) {
    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);

    proto::distributed_query::HelloRequest request{};
    proto::distributed_query::HelloResponse response{};
    ::grpc::ClientContext context{};

    auto status = stub->Hello(&context, request, &response);
    EXPECT_TRUE(status.ok()) << status.error_message();
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process) {
    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);

    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload("transaction_type=rtx\nsource_kind=scan");
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, status.error_code());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_invokes_registered_handler) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    auto handler = std::make_shared<test_remote_process_handler>();
    adapter->set_remote_process_handler(handler);

    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload("block-bytes");
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(0, response.sequence_number());
    EXPECT_EQ("block-bytes", handler->received());
    EXPECT_EQ("handled:block-bytes", response.payload());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_maps_handler_error) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<test_remote_process_handler>(false));

    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload("invalid-block");
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
    EXPECT_EQ("rejected payload", status.error_message());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_maps_unavailable_error) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<test_remote_process_handler>(
        false,
        remote_process_error_kind::unavailable));

    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload("block-bytes");
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::UNAVAILABLE, status.error_code());
    EXPECT_EQ("rejected payload", status.error_message());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_exposes_client_cancellation) {
    using namespace std::chrono_literals;
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    auto handler = std::make_shared<cancellation_test_remote_process_handler>();
    adapter->set_remote_process_handler(handler);

    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload("block-bytes");
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + 10ms);

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::DEADLINE_EXCEEDED, status.error_code());
    for (std::size_t i = 0; i < 100 && !handler->cancellation_observed(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(handler->cancellation_observed());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_rejects_missing_metadata) {
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_rejects_unknown_major_version) {
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.mutable_metadata()->mutable_protocol_version()->set_major(2);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
}

} // namespace tateyama::grpc::distributed_query
