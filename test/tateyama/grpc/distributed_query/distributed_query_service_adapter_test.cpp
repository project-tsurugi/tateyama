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
#include <future>
#include <memory>
#include <sstream>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <google/rpc/status.pb.h>

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
        entered_.set_value();
        auto const end = std::chrono::steady_clock::now() + 10s;
        while (std::chrono::steady_clock::now() < end) {
            if (is_cancelled()) {
                cancellation_observed_.store(true);
                cancelled_.set_value();
                return {false, {}, "cancelled", remote_process_error_kind::cancelled};
            }
            std::this_thread::sleep_for(1ms);
        }
        return {false, {}, "cancellation was not observed", remote_process_error_kind::internal};
    }

    [[nodiscard]] std::future<void> entered() { return entered_.get_future(); }
    [[nodiscard]] std::future<void> cancelled() { return cancelled_.get_future(); }

    [[nodiscard]] bool cancellation_observed() const noexcept {
        return cancellation_observed_.load();
    }

private:
    std::atomic_bool cancellation_observed_{};
    std::promise<void> entered_{};
    std::promise<void> cancelled_{};
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
        ss << "[distributed_query_remote]\nenabled=true\n";
        ss << "[grpc_server]\n";
        ss << "enabled=true\n";
        ss << "listen_address=" << endpoint << '\n';
        ss << "[datastore]\n";
        ss << "log_location=" << path() << '\n';
        auto cfg = std::make_shared<api::configuration::whole>(
            ss,
            std::string{test_utils::default_configuration_for_tests} + "\n[distributed_query_remote]\nenabled=false\n[distributed_query_coordinator]\nenabled=false\n");
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
    context.set_deadline(std::chrono::system_clock::now() + 15s);
    auto entered = handler->entered();
    auto cancelled = handler->cancelled();
    auto call = std::async(std::launch::async, [&] {
        return execute_remote_process(*stub, context, request, response);
    });

    EXPECT_EQ(std::future_status::ready, entered.wait_for(5s));
    // Cancel even if entry confirmation timed out, so failure cannot leave a live RPC.
    context.TryCancel();
    EXPECT_EQ(std::future_status::ready, call.wait_for(20s));
    auto status = call.get();
    EXPECT_EQ(::grpc::StatusCode::CANCELLED, status.error_code());
    EXPECT_EQ(std::future_status::ready, cancelled.wait_for(5s));
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


TEST_F(distributed_query_service_adapter_test, rejects_zero_major_version) {
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.mutable_metadata()->mutable_protocol_version()->set_major(0);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT,
              execute_remote_process(*stub, context, request, response).error_code());
}

namespace {
class fixed_result_handler final : public remote_process_handler {
public:
    explicit fixed_result_handler(remote_process_result result) : result_(std::move(result)) {}
    remote_process_result operator()(std::string_view) override { ++calls; return result_; }
    std::atomic_size_t calls{};
private:
    remote_process_result result_;
};
}

TEST_F(distributed_query_service_adapter_test, preserves_sql_error_details) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    // Valid protobuf bytes for a SQL Error's detail string (field 2).
    std::string const sql_error{"\x12\x05" "error", 7};
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{false, {}, "SQL evaluation failed", remote_process_error_kind::sql_error, sql_error}));
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_EQ(::grpc::StatusCode::UNKNOWN, status.error_code());
    google::rpc::Status details{};
    ASSERT_TRUE(details.ParseFromString(status.error_details()));
    EXPECT_EQ(static_cast<int>(status.error_code()), details.code());
    EXPECT_EQ(status.error_message(), details.message());
    ASSERT_EQ(1, details.details_size());
    EXPECT_EQ("type.googleapis.com/jogasaki.proto.sql.response.Error", details.details(0).type_url());
    EXPECT_EQ(sql_error, details.details(0).value());
}

TEST_F(distributed_query_service_adapter_test, sql_error_without_details_falls_back_to_status) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{false, {}, "SQL evaluation failed", remote_process_error_kind::sql_error}));
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::UNKNOWN, status.error_code());
    EXPECT_TRUE(status.error_details().empty());
}

TEST_F(distributed_query_service_adapter_test, accepts_messages_larger_than_default_grpc_limit) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    constexpr std::size_t size = 8U * 1024U * 1024U;
    auto handler = std::make_shared<fixed_result_handler>(remote_process_result{true, std::string(size, 'r'), {}});
    adapter->set_remote_process_handler(handler);
    ::grpc::ChannelArguments args{};
    args.SetMaxSendMessageSize(64 * 1024 * 1024);
    args.SetMaxReceiveMessageSize(64 * 1024 * 1024);
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload(std::string(size, 'q'));
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(size, response.payload().size());
    EXPECT_EQ(1U, handler->calls.load());
}

TEST_F(distributed_query_service_adapter_test, rejects_oversized_request_without_calling_handler) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    auto handler = std::make_shared<fixed_result_handler>(remote_process_result{true, "ok", {}});
    adapter->set_remote_process_handler(handler);
    ::grpc::ChannelArguments args{};
    args.SetMaxSendMessageSize(65 * 1024 * 1024);
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    request.set_payload(std::string(64U * 1024U * 1024U, 'q'));
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());
    EXPECT_EQ(0U, handler->calls.load());
}

TEST_F(distributed_query_service_adapter_test, rejects_oversized_serialized_result) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{true, std::string(64U * 1024U * 1024U, 'r'), {}}));
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());
    EXPECT_TRUE(response.payload().empty());
}


TEST_F(distributed_query_service_adapter_test, accepts_result_at_exact_serialized_limit) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    constexpr std::size_t max_size = 64U * 1024U * 1024U;
    // Field tag + four-byte length prefix, with zero sequence omitted.
    std::string payload(max_size - 5U, 'r');
    proto::distributed_query::ExecuteRemoteProcessResponse expected{};
    expected.set_payload(payload);
    ASSERT_EQ(max_size, expected.ByteSizeLong());
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{true, std::move(payload), {}}));
    ::grpc::ChannelArguments args{};
    args.SetMaxReceiveMessageSize(static_cast<int>(max_size));
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(max_size, response.ByteSizeLong());
    EXPECT_EQ(expected.payload(), response.payload());
}

TEST_F(distributed_query_service_adapter_test, maps_handler_resource_exhaustion) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{false, {}, "capacity exceeded", remote_process_error_kind::resource_exhausted}));
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    proto::distributed_query::ExecuteRemoteProcessRequest request{};
    set_valid_metadata(request);
    proto::distributed_query::ExecuteRemoteProcessResponse response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());
    EXPECT_TRUE(status.error_details().empty());
}

class distributed_query_role_test : public ::testing::Test, public test_utils::utility {
public:
    void SetUp() override { temporary_.prepare(); }
    void TearDown() override {
        if (server_) { server_->shutdown(); }
        temporary_.clean();
    }
protected:
    bool setup(std::string const& roles, bool grpc_enabled = true) {
        std::stringstream ss{};
        ss << roles << "[grpc_server]\nenabled=" << (grpc_enabled ? "true" : "false")
           << "\nlisten_address=localhost:62347\n[blob_relay]\nenabled=true\nsession_store=" << path()
           << "/blob\n[datastore]\nlog_location=" << path() << '\n';
        auto cfg = std::make_shared<api::configuration::whole>(ss, std::string{test_utils::default_configuration_for_tests} + "\n[distributed_query_remote]\nenabled=false\n[distributed_query_coordinator]\nenabled=false\n");
        set_dbpath(*cfg);
        server_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        framework::add_core_components(*server_);
        return server_->setup();
    }
    ::grpc::Status hello() {
        auto channel = ::grpc::CreateChannel("localhost:62347", ::grpc::InsecureChannelCredentials());
        auto stub = proto::distributed_query::DistributedQuery::NewStub(channel);
        proto::distributed_query::HelloRequest request{};
        proto::distributed_query::HelloResponse response{};
        ::grpc::ClientContext context{};
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{3});
        return stub->Hello(&context, request, &response);
    }
    std::unique_ptr<framework::server> server_{};
};

TEST_F(distributed_query_role_test, omitted_roles_do_not_register_remote_service) {
    ASSERT_TRUE(setup(""));
    ASSERT_TRUE(server_->start());
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, hello().error_code());
}

TEST_F(distributed_query_role_test, coordinator_only_does_not_register_remote_service) {
    ASSERT_TRUE(setup("[distributed_query_coordinator]\nenabled=true\n[distributed_query_remote]\nenabled=false\n"));
    ASSERT_TRUE(server_->start());
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, hello().error_code());
}

TEST_F(distributed_query_role_test, dual_roles_register_remote_service) {
    ASSERT_TRUE(setup("[distributed_query_coordinator]\nenabled=true\n[distributed_query_remote]\nenabled=true\n"));
    ASSERT_TRUE(server_->start());
    EXPECT_TRUE(hello().ok());
}

TEST_F(distributed_query_role_test, remote_requires_grpc) {
    EXPECT_FALSE(setup("[distributed_query_remote]\nenabled=true\n", false));
}

TEST_F(distributed_query_role_test, disabled_remote_does_not_require_grpc) {
    EXPECT_TRUE(setup("[distributed_query_remote]\nenabled=false\n", false));
}

TEST_F(distributed_query_role_test, invalid_remote_flag_is_rejected) {
    EXPECT_FALSE(setup("[distributed_query_remote]\nenabled=invalid\n"));
}

} // namespace tateyama::grpc::distributed_query
