/*
 * Copyright 2018-2026 Project Tsurugi.
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
#include <cstdint>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <google/rpc/status.pb.h>

#include <tateyama/framework/server.h>
#include <tateyama/grpc/distributed_query/catalog_service.h>
#include <tateyama/grpc/distributed_query/service_adapter.h>
#include <tateyama/grpc/server_resource.h>
#include <tateyama/proto/distributed_query/hello.grpc.pb.h>
#include <tateyama/proto/distributed_query/remote_catalog.grpc.pb.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::grpc::distributed_query {

// tateyama::grpc::proto (remote_catalog.proto) would otherwise hide tateyama::proto in this namespace.
namespace proto = ::tateyama::proto;
namespace catalog_wire = ::tateyama::grpc::proto;

namespace {

void set_test_payload(catalog_wire::RemoteProcessRequest& request, std::string_view payload) {
    auto* block = request.mutable_execution_block();
    block->set_version(1);
    block->add_operators()->mutable_source()->mutable_storage()->set_table_name(payload.data(), payload.size());
}

std::string fixture_payload(std::string_view bytes) {
    catalog_wire::RemoteExecutionBlock block{};
    if (block.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())) && block.operators_size() != 0) {
        return block.operators(0).source().storage().table_name();
    }
    return std::string{bytes};
}

class test_remote_process_handler final : public remote_process_handler {
public:
    explicit test_remote_process_handler(
        bool success = true,
        remote_process_error_kind error_kind = remote_process_error_kind::invalid_request)
        : success_(success), error_kind_(error_kind) {}

    remote_process_result operator()(std::string_view payload) override {
        received_ = fixture_payload(payload);
        if (!success_) return {false, {}, "rejected payload", error_kind_};
        return {true, "handled:" + received_, {}};
    }

    [[nodiscard]] std::string const& received() const noexcept {
        return received_;
    }

private:
    bool success_{};
    remote_process_error_kind error_kind_{};
    std::string received_{};
};

class streaming_test_handler final : public remote_process_handler {
public:
    remote_process_result operator()(std::string_view) override { return {false, {}, "stream required"}; }
    remote_process_result execute_stream(std::string_view payload, cancellation_check const&, result_sink const& sink) override {
        auto request = fixture_payload(payload);
        if (request == "throw") throw std::runtime_error("handler failure");
        if (request == "oversized-after-first") {
            if (!sink("first")) return {false, {}, "closed", remote_process_error_kind::cancelled};
            auto accepted = sink(std::string(std::size_t{64} * 1024U * 1024U, 'x'));
            return {accepted, {}, "oversized", remote_process_error_kind::cancelled};
        }
        for (auto const* value : {"first", "second", "third"}) {
            if (!sink(value)) return {false, {}, "closed", remote_process_error_kind::cancelled};
        }
        return {true, {}, {}};
    }
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
    catalog_wire::DistributedQueryService::Stub& stub,
    ::grpc::ClientContext& context,
    catalog_wire::RemoteProcessRequest const& request,
    catalog_wire::RemoteProcessResult& response)
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
        set_dbpath(*cfg);
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
    static void set_valid_metadata(catalog_wire::RemoteProcessRequest& request) {
        request.mutable_execution_block()->set_version(1);
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

namespace {
class typed_streaming_handler final : public remote_process_handler {
public:
    explicit typed_streaming_handler(bool oversized = false) : oversized_(oversized) {}
    remote_process_result operator()(std::string_view) override { return {false, {}, "record sink required"}; }
    remote_process_result execute_records(std::string_view, cancellation_check const&, record_sink const& sink) override {
        if (!sink({"first"})) return {false, {}, "closed", remote_process_error_kind::cancelled};
        if (oversized_) {
            auto accepted = sink({std::string(std::size_t{64} * 1024U * 1024U, 'x')});
            return {accepted, {}, "oversized", remote_process_error_kind::cancelled};
        }
        if (!sink({"second", "third"})) return {false, {}, "closed", remote_process_error_kind::cancelled};
        return {true, {}, {}};
    }
private:
    bool oversized_{};
};

catalog_wire::RemoteProcessRequest typed_request() {
    catalog_wire::RemoteProcessRequest request{};
    auto* metadata = request.mutable_metadata();
    metadata->mutable_protocol_version()->set_major(1);
    metadata->set_coordinator_job_id("job");
    metadata->set_remote_execution_id("execution");
    metadata->set_attempt_number(1);
    request.mutable_execution_block()->set_version(1);
    return request;
}
} // namespace

TEST_F(distributed_query_service_adapter_test, typed_rpc_streams_records_and_enforces_response_limit) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    for (bool oversized : {false, true}) {
        adapter->set_remote_process_handler(std::make_shared<typed_streaming_handler>(oversized));
        auto stub = catalog_wire::DistributedQueryService::NewStub(
            ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
        auto request = typed_request();
        ::grpc::ClientContext context{};
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
        auto reader = stub->ExecuteRemoteProcess(&context, request);
        catalog_wire::RemoteProcessResult response{};
        ASSERT_TRUE(reader->Read(&response));
        EXPECT_EQ(0U, response.sequence_number());
        ASSERT_EQ(1, response.record_list_size());
        EXPECT_EQ("first", response.record_list(0));
        if (oversized) {
            EXPECT_FALSE(reader->Read(&response));
            EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, reader->Finish().error_code());
        } else {
            ASSERT_TRUE(reader->Read(&response));
            EXPECT_EQ(1U, response.sequence_number());
            ASSERT_EQ(2, response.record_list_size());
            EXPECT_EQ("second", response.record_list(0));
            EXPECT_EQ("third", response.record_list(1));
            EXPECT_FALSE(reader->Read(&response));
            EXPECT_TRUE(reader->Finish().ok());
        }
    }
}

TEST_F(distributed_query_service_adapter_test, typed_rpc_rejects_invalid_version_and_identity) {
    auto handler = std::make_shared<test_remote_process_handler>();
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    adapter->set_remote_process_handler(handler);
    for (int failure : {0, 1, 2}) {
        auto request = typed_request();
        if (failure == 0) request.mutable_metadata()->mutable_protocol_version()->set_major(0);
        if (failure == 1) request.mutable_metadata()->mutable_protocol_version()->set_major(2);
        if (failure == 2) request.mutable_metadata()->clear_remote_execution_id();
        auto stub = catalog_wire::DistributedQueryService::NewStub(
            ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
        ::grpc::ClientContext context{};
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
        auto reader = stub->ExecuteRemoteProcess(&context, request);
        catalog_wire::RemoteProcessResult response{};
        EXPECT_FALSE(reader->Read(&response));
        EXPECT_EQ(failure == 1 ? ::grpc::StatusCode::FAILED_PRECONDITION : ::grpc::StatusCode::INVALID_ARGUMENT,
            reader->Finish().error_code());
        EXPECT_TRUE(handler->received().empty());
    }
}

TEST_F(distributed_query_service_adapter_test, typed_rpc_requires_execution_block) {
    auto handler = std::make_shared<test_remote_process_handler>();
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    adapter->set_remote_process_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    auto* metadata = request.mutable_metadata();
    metadata->mutable_protocol_version()->set_major(1);
    metadata->set_coordinator_job_id("job");
    metadata->set_remote_execution_id("execution");
    metadata->set_attempt_number(1);
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
    auto reader = stub->ExecuteRemoteProcess(&context, request);
    catalog_wire::RemoteProcessResult response{};
    EXPECT_FALSE(reader->Read(&response));
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, reader->Finish().error_code());
    EXPECT_TRUE(handler->received().empty());
}

TEST_F(distributed_query_service_adapter_test, typed_rpc_transports_complete_records) {
    auto handler = std::make_shared<test_remote_process_handler>();
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    adapter->set_remote_process_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    auto* metadata = request.mutable_metadata();
    metadata->mutable_protocol_version()->set_major(1);
    metadata->set_coordinator_job_id("job");
    metadata->set_remote_execution_id("execution");
    metadata->set_attempt_number(1);
    request.mutable_execution_block()->set_version(1);
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
    auto reader = stub->ExecuteRemoteProcess(&context, request);
    catalog_wire::RemoteProcessResult response{};
    ASSERT_TRUE(reader->Read(&response));
    EXPECT_EQ(0U, response.sequence_number());
    ASSERT_EQ(1, response.record_list_size());
    EXPECT_EQ("handled:" + request.execution_block().SerializeAsString(), response.record_list(0));
    EXPECT_FALSE(reader->Read(&response));
    EXPECT_TRUE(reader->Finish().ok());
    EXPECT_EQ(request.execution_block().SerializeAsString(), handler->received());
}

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
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);

    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "transaction_type=rtx\nsource_kind=scan");
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "block-bytes");
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(0U, response.sequence_number());
    EXPECT_EQ("block-bytes", handler->received());
    EXPECT_EQ("handled:block-bytes", response.record_list(0));
}

TEST_F(distributed_query_service_adapter_test, streams_chunks_with_contiguous_sequence) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<streaming_test_handler>());
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "stream");
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
    auto reader = stub->ExecuteRemoteProcess(&context, request);
    catalog_wire::RemoteProcessResult response{};
    std::vector<std::string> payloads{};
    while (reader->Read(&response)) {
        EXPECT_EQ(payloads.size(), response.sequence_number());
        payloads.push_back(response.record_list(0));
    }
    EXPECT_TRUE(reader->Finish().ok());
    EXPECT_EQ((std::vector<std::string>{"first", "second", "third"}), payloads);
}

TEST_F(distributed_query_service_adapter_test, oversized_later_chunk_fails_without_sending_it) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<streaming_test_handler>());
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "oversized-after-first");
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
    auto reader = stub->ExecuteRemoteProcess(&context, request);
    catalog_wire::RemoteProcessResult response{};
    ASSERT_TRUE(reader->Read(&response));
    EXPECT_EQ(0U, response.sequence_number());
    EXPECT_EQ("first", response.record_list(0));
    EXPECT_FALSE(reader->Read(&response));
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, reader->Finish().error_code());
}

TEST_F(distributed_query_service_adapter_test, handler_exception_is_internal_error) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<streaming_test_handler>());
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "throw");
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{10});
    catalog_wire::RemoteProcessResult response{};
    auto result = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::INTERNAL, result.error_code());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_maps_handler_error) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<test_remote_process_handler>(false));

    auto channel = ::grpc::CreateChannel(
        std::string{endpoint},
        ::grpc::InsecureChannelCredentials());
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "invalid-block");
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "block-bytes");
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, "block-bytes");
    catalog_wire::RemoteProcessResult response{};
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

TEST_F(distributed_query_service_adapter_test, unregister_cancels_and_drains_active_handler) {
    using namespace std::chrono_literals;
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    auto handler = std::make_shared<cancellation_test_remote_process_handler>();
    auto entered = handler->entered();
    auto cancelled = handler->cancelled();
    adapter->set_remote_process_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + 15s);
    catalog_wire::RemoteProcessResult response{};
    auto call = std::async(std::launch::async, [&] {
        return execute_remote_process(*stub, context, request, response);
    });
    EXPECT_EQ(std::future_status::ready, entered.wait_for(5s));
    adapter->set_remote_process_handler({});
    // The clear operation has returned only after cancellation reached the handler.
    EXPECT_EQ(std::future_status::ready, cancelled.wait_for(0s));
    EXPECT_TRUE(handler->cancellation_observed());
    EXPECT_EQ(std::future_status::ready, call.wait_for(20s));
    EXPECT_EQ(::grpc::StatusCode::CANCELLED, call.get().error_code());
    ::grpc::ClientContext next_context{};
    next_context.set_deadline(std::chrono::system_clock::now() + 5s);
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED,
        execute_remote_process(*stub, next_context, request, response).error_code());
}

TEST_F(distributed_query_service_adapter_test, unregister_cancels_unread_stream_and_releases_handler) {
    using namespace std::chrono_literals;
    struct flooding_handler final : remote_process_handler {
        std::promise<void> entered{};
        std::atomic_bool sink_closed{};
        remote_process_result operator()(std::string_view) override { return {false, {}, "unused"}; }
        remote_process_result execute_stream(std::string_view, cancellation_check const&, result_sink const& sink) override {
            std::string chunk(2U * 1024U * 1024U, 'r');
            entered.set_value();
            for (std::size_t i = 0; i < 64; ++i) {
                if (!sink(chunk)) {
                    sink_closed.store(true);
                    return {false, {}, "closed", remote_process_error_kind::cancelled};
                }
            }
            return {true, {}, {}};
        }
    };
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    auto handler = std::make_shared<flooding_handler>();
    std::weak_ptr<flooding_handler> weak = handler;
    auto entered = handler->entered.get_future();
    adapter->set_remote_process_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + 15s);
    auto reader = stub->ExecuteRemoteProcess(&context, request);
    EXPECT_EQ(std::future_status::ready, entered.wait_for(5s));
    // Leave the consumer unread while the server fills the transport's bounded buffers.
    std::this_thread::sleep_for(100ms);
    auto clear = std::async(std::launch::async, [&] { adapter->set_remote_process_handler({}); });
    auto finished = clear.wait_for(5s);
    EXPECT_EQ(std::future_status::ready, finished);
    // Cleanup also on failure; this fallback cannot turn a timed-out clear into a passing test.
    context.TryCancel();
    ASSERT_EQ(std::future_status::ready, clear.wait_for(10s));
    clear.get();
    EXPECT_TRUE(handler->sink_closed.load());
    EXPECT_EQ(::grpc::StatusCode::CANCELLED, reader->Finish().error_code());
    handler.reset();
    EXPECT_TRUE(weak.expired());
}

TEST_F(distributed_query_service_adapter_test, unregister_waits_until_handler_returns) {
    using namespace std::chrono_literals;
    struct gated_handler final : remote_process_handler {
        std::promise<void> entered{};
        std::promise<void> cancelled{};
        std::promise<void> release{};
        std::shared_future<void> continue_execution{release.get_future().share()};
        std::atomic_bool exited{};
        remote_process_result operator()(std::string_view) override { return {false, {}, "unused"}; }
        remote_process_result execute(std::string_view, cancellation_check const& check) override {
            entered.set_value();
            auto end = std::chrono::steady_clock::now() + 10s;
            while (!check() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
            cancelled.set_value();
            (void) continue_execution.wait_for(5s);
            exited.store(true);
            return {false, {}, "cancelled", remote_process_error_kind::cancelled};
        }
    };
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    auto handler = std::make_shared<gated_handler>();
    auto entered = handler->entered.get_future();
    auto cancelled = handler->cancelled.get_future();
    adapter->set_remote_process_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + 20s);
    catalog_wire::RemoteProcessResult response{};
    auto call = std::async(std::launch::async, [&] {
        return execute_remote_process(*stub, context, request, response);
    });
    EXPECT_EQ(std::future_status::ready, entered.wait_for(5s));
    auto clear = std::async(std::launch::async, [&] { adapter->set_remote_process_handler({}); });
    EXPECT_EQ(std::future_status::ready, cancelled.wait_for(5s));
    EXPECT_EQ(std::future_status::timeout, clear.wait_for(50ms));
    EXPECT_FALSE(handler->exited.load());
    handler->release.set_value();
    EXPECT_EQ(std::future_status::ready, clear.wait_for(10s));
    clear.get();
    EXPECT_TRUE(handler->exited.load());
    EXPECT_EQ(std::future_status::ready, call.wait_for(20s));
    EXPECT_EQ(::grpc::StatusCode::CANCELLED, call.get().error_code());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_rejects_missing_metadata) {
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, status.error_code());
}

TEST_F(distributed_query_service_adapter_test, execute_remote_process_rejects_unknown_major_version) {
    auto channel = ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials());
    auto stub = catalog_wire::DistributedQueryService::NewStub(channel);
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    request.mutable_metadata()->mutable_protocol_version()->set_major(2);
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};

    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
}


TEST_F(distributed_query_service_adapter_test, rejects_zero_major_version) {
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    request.mutable_metadata()->mutable_protocol_version()->set_major(0);
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, std::string(size, 'q'));
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(size, response.record_list(0).size());
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    set_test_payload(request, std::string(64U * 1024U * 1024U, 'q'));
    catalog_wire::RemoteProcessResult response{};
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
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());
    EXPECT_EQ(0, response.record_list_size());
}


TEST_F(distributed_query_service_adapter_test, accepts_result_at_exact_serialized_limit) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    constexpr std::size_t max_size = 64U * 1024U * 1024U;
    // Field tag + four-byte length prefix, with zero sequence omitted.
    std::string payload(max_size - 5U, 'r');
    catalog_wire::RemoteProcessResult expected{};
    expected.add_record_list(payload);
    ASSERT_EQ(max_size, expected.ByteSizeLong());
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{true, std::move(payload), {}}));
    ::grpc::ChannelArguments args{};
    args.SetMaxReceiveMessageSize(static_cast<int>(max_size));
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(max_size, response.ByteSizeLong());
    EXPECT_EQ(expected.record_list(0), response.record_list(0));
}

TEST_F(distributed_query_service_adapter_test, maps_handler_resource_exhaustion) {
    auto adapter = std::dynamic_pointer_cast<service_adapter>(
        server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    ASSERT_TRUE(adapter);
    adapter->set_remote_process_handler(std::make_shared<fixed_result_handler>(
        remote_process_result{false, {}, "capacity exceeded", remote_process_error_kind::resource_exhausted}));
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    catalog_wire::RemoteProcessRequest request{};
    set_valid_metadata(request);
    catalog_wire::RemoteProcessResult response{};
    ::grpc::ClientContext context{};
    auto status = execute_remote_process(*stub, context, request, response);
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, status.error_code());
    EXPECT_TRUE(status.error_details().empty());
}

namespace {

class fixed_catalog_handler final : public remote_catalog_handler {
public:
    explicit fixed_catalog_handler(remote_catalog_result result) : result_(std::move(result)) {}
    remote_catalog_result operator()() override {
        ++calls;
        if (throws) throw std::runtime_error("catalog handler failure");
        return result_;
    }
    std::atomic_size_t calls{};
    bool throws{};
private:
    remote_catalog_result result_;
};

class blocking_catalog_handler final : public remote_catalog_handler {
public:
    explicit blocking_catalog_handler(std::string catalog) : catalog_(std::move(catalog)) {}
    remote_catalog_result operator()() override {
        entered_.set_value();
        release_.get_future().wait();
        return {true, catalog_, {}};
    }
    std::future<void> entered() { return entered_.get_future(); }
    void release() { release_.set_value(); }
private:
    std::string catalog_;
    std::promise<void> entered_{};
    std::promise<void> release_{};
};

catalog_wire::RemoteCatalogRequest valid_catalog_request() {
    catalog_wire::RemoteCatalogRequest request{};
    request.mutable_metadata()->mutable_protocol_version()->set_major(1);
    request.mutable_metadata()->mutable_protocol_version()->set_minor(7);
    request.mutable_metadata()->set_request_id("catalog-request");
    return request;
}

// one table with two columns, an index and functions; indices refer to the complete arrays
catalog_wire::RemoteCatalog sample_catalog() {
    catalog_wire::RemoteCatalog catalog{};
    auto* schema = catalog.add_schema_list();
    schema->set_schema_name("public");
    schema->add_table_index_list(0);
    schema->add_index_index_list(0);
    schema->add_function_index_list(0);
    schema->add_aggregate_function_index_list(0);
    auto* table = catalog.add_table_list();
    table->set_remote_definition_id(11);
    table->set_table_name("t");
    for (std::uint64_t c = 0; c < 2; ++c) {
        table->add_column_index_list(c);
        auto* column = catalog.add_column_list();
        column->set_column_name("c" + std::to_string(c));
        column->mutable_data_type()->mutable_int8_type();
        column->set_nullable(c != 0);
    }
    auto* index = catalog.add_index_list();
    index->set_remote_definition_id(12);
    index->set_index_name("pk");
    index->set_table_index(0);
    index->add_features(catalog_wire::REMOTE_INDEX_FEATURE_PRIMARY);
    auto* key = index->add_keys();
    key->set_column_index(0);
    key->set_direction(catalog_wire::REMOTE_SORT_DIRECTION_ASCENDING);
    index->add_value_column_index_list(1);
    auto* function = catalog.add_function_list();
    function->set_remote_definition_id(13);
    function->set_function_name("f");
    function->add_parameter_types()->mutable_int4_type();
    function->mutable_return_type()->mutable_decimal_type()->set_precision(5);
    function->add_features(catalog_wire::REMOTE_FUNCTION_FEATURE_SCALAR);
    auto* aggregate = catalog.add_aggregate_function_list();
    aggregate->set_remote_definition_id(14);
    aggregate->set_function_name("agg");
    aggregate->add_parameter_types()->mutable_int8_type();
    aggregate->mutable_return_type()->mutable_int8_type();
    aggregate->set_incremental(true);
    return catalog;
}

catalog_wire::RemoteCatalog merge(std::vector<catalog_wire::RemoteCatalogResponse> const& responses) {
    catalog_wire::RemoteCatalog merged{};
    for (auto const& response : responses) {
        merged.MergeFrom(response.catalog_fragment());
    }
    return merged;
}

std::size_t element_count(catalog_wire::RemoteCatalog const& c) {
    return static_cast<std::size_t>(c.schema_list_size() + c.table_list_size() + c.column_list_size() +
        c.index_list_size() + c.function_list_size() + c.aggregate_function_list_size());
}

} // namespace

class remote_catalog_service_test : public distributed_query_service_adapter_test {
protected:
    std::shared_ptr<service_adapter> adapter() {
        return std::dynamic_pointer_cast<service_adapter>(
            server_->find_resource_by_id(framework::resource_id_distributed_query_service));
    }

    ::grpc::Status get_catalog(
        catalog_wire::RemoteCatalogRequest const& request,
        std::vector<catalog_wire::RemoteCatalogResponse>& responses)
    {
        ::grpc::ChannelArguments args{};
        args.SetMaxReceiveMessageSize(static_cast<int>(remote_catalog_max_message_size));
        auto stub = catalog_wire::DistributedQueryService::NewStub(
            ::grpc::CreateCustomChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials(), args));
        ::grpc::ClientContext context{};
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{30});
        auto reader = stub->GetRemoteCatalog(&context, request);
        catalog_wire::RemoteCatalogResponse response{};
        while (reader->Read(&response)) {
            responses.push_back(response);
        }
        return reader->Finish();
    }
};

TEST_F(remote_catalog_service_test, unimplemented_without_handler) {
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    auto status = get_catalog(valid_catalog_request(), responses);
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, status.error_code());
    EXPECT_EQ("remote catalog handler is not registered", status.error_message());
    EXPECT_TRUE(responses.empty());
}

TEST_F(remote_catalog_service_test, streams_complete_catalog_and_keeps_execution_rpc) {
    auto handler = std::make_shared<fixed_catalog_handler>(
        remote_catalog_result{true, sample_catalog().SerializeAsString(), {}});
    adapter()->set_remote_catalog_handler(handler);
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    auto status = get_catalog(valid_catalog_request(), responses);
    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(1U, responses.size());
    EXPECT_EQ(0U, responses[0].sequence_number());
    auto restored = merge(responses);
    EXPECT_EQ(sample_catalog().SerializeAsString(), restored.SerializeAsString());
    // optional indices keep "set to 0" distinct from "absent"
    EXPECT_EQ(catalog_wire::RemoteIndex::kTableIndex, restored.index_list(0).table_index_presence_case());
    EXPECT_EQ(0U, restored.index_list(0).table_index());
    EXPECT_EQ(catalog_wire::RemoteIndexKey::kColumnIndex, restored.index_list(0).keys(0).column_index_presence_case());
    EXPECT_EQ(catalog_wire::RemoteDecimalType::kPrecision,
        restored.function_list(0).return_type().decimal_type().precision_presence_case());
    EXPECT_EQ(catalog_wire::RemoteDecimalType::SCALE_PRESENCE_NOT_SET,
        restored.function_list(0).return_type().decimal_type().scale_presence_case());
    EXPECT_EQ(1U, handler->calls.load());

    // the existing execution service is still served next to the catalog service
    auto stub = proto::distributed_query::DistributedQuery::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    proto::distributed_query::HelloRequest hello{};
    proto::distributed_query::HelloResponse hello_response{};
    ::grpc::ClientContext context{};
    EXPECT_TRUE(stub->Hello(&context, hello, &hello_response).ok());
}

TEST_F(remote_catalog_service_test, definition_ids_preserve_zero_and_absence_over_rpc) {
    for (bool table_id_present : {false, true}) {
        for (bool index_id_present : {false, true}) {
            SCOPED_TRACE(::testing::Message{} << "table ID present=" << table_id_present
                                             << ", index ID present=" << index_id_present);
            auto catalog = sample_catalog();
            auto* table = catalog.mutable_table_list(0);
            auto* index = catalog.mutable_index_list(0);
            table->clear_remote_definition_id();
            index->clear_remote_definition_id();
            if (table_id_present) table->set_remote_definition_id(0);
            if (index_id_present) index->set_remote_definition_id(0);
            auto expected = catalog.SerializeAsString();
            auto handler = std::make_shared<fixed_catalog_handler>(remote_catalog_result{true, expected, {}});
            adapter()->set_remote_catalog_handler(handler);
            std::vector<catalog_wire::RemoteCatalogResponse> responses{};
            auto status = get_catalog(valid_catalog_request(), responses);
            ASSERT_TRUE(status.ok()) << status.error_message();
            ASSERT_EQ(1U, responses.size());
            EXPECT_EQ(0U, responses[0].sequence_number());
            auto restored = merge(responses);
            ASSERT_EQ(1, restored.table_list_size());
            ASSERT_EQ(1, restored.index_list_size());
            EXPECT_EQ(table_id_present ? catalog_wire::RemoteTable::kRemoteDefinitionId
                                      : catalog_wire::RemoteTable::REMOTE_DEFINITION_ID_PRESENCE_NOT_SET,
                restored.table_list(0).remote_definition_id_presence_case());
            EXPECT_EQ(index_id_present ? catalog_wire::RemoteIndex::kRemoteDefinitionId
                                      : catalog_wire::RemoteIndex::REMOTE_DEFINITION_ID_PRESENCE_NOT_SET,
                restored.index_list(0).remote_definition_id_presence_case());
            EXPECT_EQ(0U, restored.table_list(0).remote_definition_id());
            EXPECT_EQ(0U, restored.index_list(0).remote_definition_id());
            EXPECT_EQ(expected, restored.SerializeAsString());
            EXPECT_EQ(1U, handler->calls.load());
        }
    }
}

TEST_F(remote_catalog_service_test, empty_catalog_is_zero_messages_and_ok) {
    adapter()->set_remote_catalog_handler(std::make_shared<fixed_catalog_handler>(remote_catalog_result{true, {}, {}}));
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    auto status = get_catalog(valid_catalog_request(), responses);
    EXPECT_TRUE(status.ok()) << status.error_message();
    EXPECT_TRUE(responses.empty());
}

TEST_F(remote_catalog_service_test, rejects_invalid_metadata_without_calling_handler) {
    auto handler = std::make_shared<fixed_catalog_handler>(
        remote_catalog_result{true, sample_catalog().SerializeAsString(), {}});
    adapter()->set_remote_catalog_handler(handler);
    std::vector<std::pair<catalog_wire::RemoteCatalogRequest, ::grpc::StatusCode>> cases{};
    cases.emplace_back(catalog_wire::RemoteCatalogRequest{}, ::grpc::StatusCode::INVALID_ARGUMENT);
    {
        auto request = valid_catalog_request();
        request.mutable_metadata()->clear_protocol_version();
        cases.emplace_back(request, ::grpc::StatusCode::INVALID_ARGUMENT);
    }
    {
        auto request = valid_catalog_request();
        request.mutable_metadata()->clear_request_id();
        cases.emplace_back(request, ::grpc::StatusCode::INVALID_ARGUMENT);
    }
    for (std::uint32_t major : {0U, 2U}) {
        auto request = valid_catalog_request();
        request.mutable_metadata()->mutable_protocol_version()->set_major(major);
        cases.emplace_back(request, ::grpc::StatusCode::FAILED_PRECONDITION);
    }
    for (auto const& [request, expected] : cases) {
        std::vector<catalog_wire::RemoteCatalogResponse> responses{};
        EXPECT_EQ(expected, get_catalog(request, responses).error_code());
        EXPECT_TRUE(responses.empty());
    }
    EXPECT_EQ(0U, handler->calls.load());
}

TEST_F(remote_catalog_service_test, handler_failures_send_no_response) {
    std::vector<std::pair<std::shared_ptr<fixed_catalog_handler>, ::grpc::StatusCode>> cases{};
    cases.emplace_back(std::make_shared<fixed_catalog_handler>(remote_catalog_result{
        false, {}, "bit column", remote_catalog_error_kind::unsupported}), ::grpc::StatusCode::UNIMPLEMENTED);
    cases.emplace_back(std::make_shared<fixed_catalog_handler>(remote_catalog_result{
        false, {}, "provider failed", remote_catalog_error_kind::internal}), ::grpc::StatusCode::INTERNAL);
    cases.emplace_back(std::make_shared<fixed_catalog_handler>(remote_catalog_result{
        true, std::string{"\xff\xff\xff", 3}, {}}), ::grpc::StatusCode::INTERNAL);
    auto throwing = std::make_shared<fixed_catalog_handler>(remote_catalog_result{true, {}, {}});
    throwing->throws = true;
    cases.emplace_back(throwing, ::grpc::StatusCode::INTERNAL);
    for (auto const& [handler, expected] : cases) {
        adapter()->set_remote_catalog_handler(handler);
        std::vector<catalog_wire::RemoteCatalogResponse> responses{};
        auto status = get_catalog(valid_catalog_request(), responses);
        EXPECT_EQ(expected, status.error_code()) << status.error_message();
        EXPECT_TRUE(responses.empty());
        EXPECT_EQ(1U, handler->calls.load());
    }
}

TEST_F(remote_catalog_service_test, cleared_handler_is_unimplemented_again) {
    adapter()->set_remote_catalog_handler(std::make_shared<fixed_catalog_handler>(remote_catalog_result{true, {}, {}}));
    adapter()->set_remote_catalog_handler({});
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, get_catalog(valid_catalog_request(), responses).error_code());
}

TEST_F(remote_catalog_service_test, large_catalog_is_split_within_message_limit) {
    // two 40 MiB elements cannot share one 64 MiB response
    catalog_wire::RemoteCatalog catalog{};
    for (auto i = 0; i < 2; ++i) {
        auto* column = catalog.add_column_list();
        column->set_column_name(std::string(std::size_t{40} * 1024U * 1024U, static_cast<char>('a' + i)));
        column->mutable_data_type()->mutable_int8_type();
        auto* table = catalog.add_table_list();
        table->set_table_name("t" + std::to_string(i));
        table->add_column_index_list(i);
        auto* index = catalog.add_index_list();
        index->set_index_name("pk" + std::to_string(i));
        index->set_table_index(i);
        if (i == 0) {
            table->set_remote_definition_id(0);
            index->set_remote_definition_id(0);
        }
    }
    auto expected = catalog.SerializeAsString();
    adapter()->set_remote_catalog_handler(std::make_shared<fixed_catalog_handler>(remote_catalog_result{true, expected, {}}));
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    auto status = get_catalog(valid_catalog_request(), responses);
    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(2U, responses.size());
    std::size_t elements{};
    for (std::size_t i = 0; i < responses.size(); ++i) {
        EXPECT_EQ(i, responses[i].sequence_number());
        EXPECT_LE(responses[i].ByteSizeLong(), remote_catalog_max_message_size);
        EXPECT_EQ(1, responses[i].catalog_fragment().column_list_size());
        EXPECT_GT(element_count(responses[i].catalog_fragment()), 0U);
        elements += element_count(responses[i].catalog_fragment());
    }
    EXPECT_EQ(element_count(catalog), elements);
    auto restored = merge(responses);
    ASSERT_EQ(2, restored.table_list_size());
    ASSERT_EQ(2, restored.index_list_size());
    EXPECT_EQ(catalog_wire::RemoteTable::kRemoteDefinitionId,
        restored.table_list(0).remote_definition_id_presence_case());
    EXPECT_EQ(catalog_wire::RemoteIndex::kRemoteDefinitionId,
        restored.index_list(0).remote_definition_id_presence_case());
    EXPECT_EQ(0U, restored.table_list(0).remote_definition_id());
    EXPECT_EQ(0U, restored.index_list(0).remote_definition_id());
    EXPECT_EQ(catalog_wire::RemoteTable::REMOTE_DEFINITION_ID_PRESENCE_NOT_SET,
        restored.table_list(1).remote_definition_id_presence_case());
    EXPECT_EQ(catalog_wire::RemoteIndex::REMOTE_DEFINITION_ID_PRESENCE_NOT_SET,
        restored.index_list(1).remote_definition_id_presence_case());
    EXPECT_EQ(expected, restored.SerializeAsString());
}

TEST_F(remote_catalog_service_test, single_oversized_element_is_resource_exhausted_without_response) {
    catalog_wire::RemoteCatalog catalog{};
    catalog.add_schema_list()->set_schema_name("small");
    catalog.add_column_list()->set_column_name(std::string(remote_catalog_max_message_size, 'x'));
    adapter()->set_remote_catalog_handler(std::make_shared<fixed_catalog_handler>(
        remote_catalog_result{true, catalog.SerializeAsString(), {}}));
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, get_catalog(valid_catalog_request(), responses).error_code());
    // the earlier small element is not sent either
    EXPECT_TRUE(responses.empty());
}

TEST_F(remote_catalog_service_test, client_cancellation_ends_rpc) {
    using namespace std::chrono_literals;
    auto handler = std::make_shared<blocking_catalog_handler>(sample_catalog().SerializeAsString());
    adapter()->set_remote_catalog_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + 15s);
    auto entered = handler->entered();
    auto call = std::async(std::launch::async, [&] {
        auto reader = stub->GetRemoteCatalog(&context, valid_catalog_request());
        catalog_wire::RemoteCatalogResponse response{};
        std::size_t count{};
        while (reader->Read(&response)) ++count;
        return std::pair{reader->Finish(), count};
    });
    EXPECT_EQ(std::future_status::ready, entered.wait_for(5s));
    context.TryCancel();
    handler->release();
    ASSERT_EQ(std::future_status::ready, call.wait_for(20s));
    auto [status, count] = call.get();
    EXPECT_EQ(::grpc::StatusCode::CANCELLED, status.error_code());
    EXPECT_EQ(0U, count);
}

TEST_F(remote_catalog_service_test, unregister_waits_for_catalog_handler_before_releasing_database) {
    using namespace std::chrono_literals;
    auto handler = std::make_shared<blocking_catalog_handler>(sample_catalog().SerializeAsString());
    auto resource = adapter();
    resource->set_remote_catalog_handler(handler);
    auto stub = catalog_wire::DistributedQueryService::NewStub(
        ::grpc::CreateChannel(std::string{endpoint}, ::grpc::InsecureChannelCredentials()));
    ::grpc::ClientContext context{};
    context.set_deadline(std::chrono::system_clock::now() + 15s);
    auto entered = handler->entered();
    auto call = std::async(std::launch::async, [&] {
        auto reader = stub->GetRemoteCatalog(&context, valid_catalog_request());
        catalog_wire::RemoteCatalogResponse response{};
        while (reader->Read(&response)) {}
        return reader->Finish();
    });
    EXPECT_EQ(std::future_status::ready, entered.wait_for(5s));
    std::promise<void> clearing{};
    auto started = clearing.get_future();
    auto clear = std::async(std::launch::async, [&] {
        clearing.set_value();
        resource->set_remote_catalog_handler({});
    });
    EXPECT_EQ(std::future_status::ready, started.wait_for(5s));
    EXPECT_EQ(std::future_status::timeout, clear.wait_for(50ms));
    handler->release();
    ASSERT_EQ(std::future_status::ready, clear.wait_for(10s));
    clear.get();
    ASSERT_EQ(std::future_status::ready, call.wait_for(10s));
    EXPECT_EQ(::grpc::StatusCode::CANCELLED, call.get().error_code());
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, get_catalog(valid_catalog_request(), responses).error_code());
    EXPECT_TRUE(responses.empty());
}

TEST(remote_catalog_split_test, fragments_are_contiguous_non_empty_and_sequenced) {
    auto original = sample_catalog();
    // grow every field so that a small limit forces many fragments, including splits inside one field
    for (auto i = 0; i < 20; ++i) {
        original.add_column_list()->set_column_name("extra" + std::to_string(i));
    }
    auto source = original;
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    constexpr std::size_t limit = 64;
    ASSERT_TRUE(split_remote_catalog(source, responses, limit).ok());
    ASSERT_GT(responses.size(), 3U);
    std::size_t elements{};
    for (std::size_t i = 0; i < responses.size(); ++i) {
        EXPECT_EQ(i, responses[i].sequence_number());
        EXPECT_TRUE(responses[i].has_catalog_fragment());
        EXPECT_GT(element_count(responses[i].catalog_fragment()), 0U);
        EXPECT_LE(responses[i].ByteSizeLong(), limit);
        elements += element_count(responses[i].catalog_fragment());
    }
    EXPECT_EQ(element_count(original), elements);
    EXPECT_EQ(original.SerializeAsString(), merge(responses).SerializeAsString());
}

TEST(remote_catalog_split_test, response_at_exact_limit_is_accepted) {
    catalog_wire::RemoteCatalog source{};
    source.add_schema_list()->set_schema_name(std::string(100, 's'));
    catalog_wire::RemoteCatalogResponse expected{};
    *expected.mutable_catalog_fragment() = source;
    auto exact = expected.ByteSizeLong();
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    {
        auto copy = source;
        ASSERT_TRUE(split_remote_catalog(copy, responses, exact).ok());
        ASSERT_EQ(1U, responses.size());
        EXPECT_EQ(exact, responses[0].ByteSizeLong());
    }
    {
        auto copy = source;
        responses.clear();
        EXPECT_EQ(::grpc::StatusCode::RESOURCE_EXHAUSTED, split_remote_catalog(copy, responses, exact - 1).error_code());
        EXPECT_TRUE(responses.empty());
    }
}

TEST(remote_catalog_split_test, empty_catalog_has_no_fragment) {
    catalog_wire::RemoteCatalog source{};
    std::vector<catalog_wire::RemoteCatalogResponse> responses{};
    ASSERT_TRUE(split_remote_catalog(source, responses).ok());
    EXPECT_TRUE(responses.empty());
}

class fail_once_setup_resource final : public framework::resource {
public:
    bool setup(framework::environment&) override { return attempts_++ > 0; }
    bool start(framework::environment&) override { return true; }
    bool shutdown(framework::environment&) override { return true; }
    [[nodiscard]] id_type id() const noexcept override { return max_system_reserved_id + 1; }
    [[nodiscard]] std::string_view label() const noexcept override { return "fail_once_setup_resource"; }
private:
    unsigned attempts_{};
};

class distributed_query_role_test : public ::testing::Test, public test_utils::utility {
public:
    void SetUp() override { temporary_.prepare(); }
    void TearDown() override {
        if (server_) { server_->shutdown(); }
        temporary_.clean();
    }
protected:
    bool setup(std::string const& roles, bool grpc_enabled = true, bool fail_once = false) {
        std::stringstream ss{};
        ss << roles << "[grpc_server]\nenabled=" << (grpc_enabled ? "true" : "false")
           << "\nlisten_address=localhost:62347\n[blob_relay]\nenabled=true\nsession_store=" << path()
           << "/blob\n[datastore]\nlog_location=" << path() << '\n';
        auto cfg = std::make_shared<api::configuration::whole>(ss, std::string{test_utils::default_configuration_for_tests} + "\n[distributed_query_remote]\nenabled=false\n[distributed_query_coordinator]\nenabled=false\n");
        set_dbpath(*cfg);
        server_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        if (fail_once) {
            // Isolate this adapter's retry lifecycle from other core services.
            server_->add_resource(std::make_shared<tateyama::grpc::grpc_server_resource>());
            server_->add_resource(std::make_shared<service_adapter>());
            server_->add_resource(std::make_shared<fail_once_setup_resource>());
        } else {
            framework::add_core_components(*server_);
        }
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
    // INVALID_ARGUMENT proves the catalog service is registered; an unregistered service is UNIMPLEMENTED
    ::grpc::StatusCode catalog_probe() {
        auto stub = catalog_wire::DistributedQueryService::NewStub(
            ::grpc::CreateChannel("localhost:62347", ::grpc::InsecureChannelCredentials()));
        catalog_wire::RemoteCatalogRequest request{};
        ::grpc::ClientContext context{};
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds{3});
        auto reader = stub->GetRemoteCatalog(&context, request);
        catalog_wire::RemoteCatalogResponse response{};
        while (reader->Read(&response)) {
        }
        return reader->Finish().error_code();
    }
    std::unique_ptr<framework::server> server_{};
};

TEST_F(distributed_query_role_test, omitted_roles_do_not_register_remote_service) {
    ASSERT_TRUE(setup(""));
    ASSERT_TRUE(server_->start());
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, hello().error_code());
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, catalog_probe());
}

TEST_F(distributed_query_role_test, coordinator_only_does_not_register_remote_service) {
    ASSERT_TRUE(setup("[distributed_query_coordinator]\nenabled=true\n[distributed_query_remote]\nenabled=false\n"));
    ASSERT_TRUE(server_->start());
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, hello().error_code());
    EXPECT_EQ(::grpc::StatusCode::UNIMPLEMENTED, catalog_probe());
}

TEST_F(distributed_query_role_test, dual_roles_register_remote_service) {
    ASSERT_TRUE(setup("[distributed_query_coordinator]\nenabled=true\n[distributed_query_remote]\nenabled=true\n"));
    ASSERT_TRUE(server_->start());
    EXPECT_TRUE(hello().ok());
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, catalog_probe());
}

TEST_F(distributed_query_role_test, setup_retry_preserves_registered_remote_service) {
    ASSERT_FALSE(setup("[distributed_query_remote]\nenabled=true\n", true, true));
    // start() retries setup after a later resource failed and shutdown ran.
    ASSERT_TRUE(server_->start());
    EXPECT_TRUE(hello().ok());
    EXPECT_EQ(::grpc::StatusCode::INVALID_ARGUMENT, catalog_probe());
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
