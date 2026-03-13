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

#include <functional>
#include <optional>

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

#include "ipc_client.h"
#include "ipc_gtest_base.h"
#include "grpc/client.h"

namespace tateyama::server {
class ipc_listener_for_test {
public:
    static void run(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        worker.invoke([&]{
            worker.run();
            worker.delete_hook();
        });
    }
    static void wait(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        while (!worker.is_terminated());
    }
};
}  // namespace tateyama::server

namespace tateyama::endpoint::ipc {
static constexpr std::size_t my_session_id = 123;

static constexpr std::string_view database_name = "ipc_blob_relay_streaming_test";
static constexpr std::string_view label = "label_fot_test";
static constexpr std::string_view application_name = "application_name_fot_test";
static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::size_t service_id_of_blob_relay_streaming_test_service = 200;
static constexpr std::string_view channel_name = "gRPC-blob";
static constexpr std::size_t blob_size = 5000;

class blob_relay_streaming_test_service : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) override { return true; }
    bool start(tateyama::framework::environment&) override { return true; }
    bool shutdown(tateyama::framework::environment&) override { return true; }
    std::string_view label() const noexcept override { return __func__; }

    id_type id() const noexcept override { return service_id_of_blob_relay_streaming_test_service; }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        visit_++;
        req_ = std::dynamic_pointer_cast<tateyama::endpoint::common::request>(req);
        res->body(response_test_message);

        EXPECT_TRUE(req->has_blob(channel_name));

        auto& bi = req->get_blob(channel_name);
        blobs_.emplace(channel_name, std::filesystem::path(bi.path()), bi.is_temporary());
        return true;
    }

    std::shared_ptr<tateyama::endpoint::common::request> request() {
        return req_;
    }
    std::size_t visit() {
        return visit_;
    }
    std::set<std::tuple<std::string, std::filesystem::path, bool>>& blobs() {
        return blobs_;
    }

private:
    std::size_t visit_{};
    std::shared_ptr<tateyama::endpoint::common::request> req_{};
    std::set<std::tuple<std::string, std::filesystem::path, bool>> blobs_{};
};

class ipc_blob_relay_streaming_test_server_client: public server_client_gtest_base {
public:
    ipc_blob_relay_streaming_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::function<void(ipc_client&)> f)
        : server_client_gtest_base(cfg), f_(std::move(f)) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        service_ = std::make_shared<blob_relay_streaming_test_service>();
        return service_;
    }

    void server() override {
        server_client_base::server(
            [](tateyama::framework::server& sv, std::shared_ptr<tateyama::api::configuration::whole> const & cfg){}, true);
    }

    void client_thread() override {
        ipc_client client { cfg_, true };  // client that skips administrators operation
        f_(client);
    }

    std::shared_ptr<blob_relay_streaming_test_service> service() {
        return service_;
    }

private:
    std::function<void(ipc_client&)> f_;
    std::shared_ptr<blob_relay_streaming_test_service> service_;
};

class ipc_blob_relay_streaming_test: public ipc_gtest_base {
    void SetUp() override {
        ipc_test_env::setup(
            "[grpc_server]\n"
                "enabled=true\n"
                "listen_address=0.0.0.0:62345\n"
                "endpoint=dns:///localhost:62345\n"
                "secure=false\n"
                "fullchain_crt=\n"
                "server_key=\n"
            "[blob_relay]\n"
                "enabled=true\n"
                "session_store=var/blob/sessions\n"
                "session_quota_size=\n"
                "local_enabled=false\n"
                "local_upload_copy_file=false\n"
                "stream_chunk_size=1048576\n"
        );
    }
    void TearDown() override {
        ipc_test_env::teardown();
    }

protected:
    constexpr static std::uint64_t ENDPOINT_BROKER_SERVICE_MESSAGE_VERSION_MAJOR = 0;
    constexpr static std::uint64_t ENDPOINT_BROKER_SERVICE_MESSAGE_VERSION_MINOR = 2;

    using blobs_type = std::set<std::tuple<std::string, std::uint64_t, std::uint64_t, bool>>;
    std::unique_ptr<grpc::Client> grpc_client_{};

    std::function<void(ipc_client&)> handshake_ = [this](ipc_client& client){
        tateyama::proto::endpoint::request::Request endpoint_request{};

        endpoint_request.set_service_message_version_major(ENDPOINT_BROKER_SERVICE_MESSAGE_VERSION_MAJOR);
        endpoint_request.set_service_message_version_minor(ENDPOINT_BROKER_SERVICE_MESSAGE_VERSION_MINOR);

        auto* endpoint_handshake = endpoint_request.mutable_handshake();
        endpoint_handshake->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::BLOB_RELAY_STREAMING);
        
        auto* client_information = endpoint_handshake->mutable_client_information();
        client_information->set_connection_label("ipc_blob_relay_streaming_test");

        auto* wire_information = endpoint_handshake->mutable_wire_information();
        wire_information->mutable_ipc_information();
        
        client.send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());

        tateyama::proto::framework::response::Header::PayloadType type{};
        tateyama::proto::endpoint::response::Handshake handshake_response{};

        std::string res{};
        client.receive(res, type);
        if(!handshake_response.ParseFromString(res)) {
            FAIL();
        }

        // check handshake response
        EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
        EXPECT_EQ(handshake_response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
        auto& success = handshake_response.success();
        EXPECT_EQ(success.blob_transfer_info_opt_case(), tateyama::proto::endpoint::response::Handshake_Success::kBlobRelayInfo);
        auto& blob_relay_info = success.blob_relay_info();
        EXPECT_EQ(blob_relay_info.endpoint(), "dns:///localhost:62345");
        EXPECT_EQ(blob_relay_info.secure(), false);

        grpc_client_ = std::make_unique<grpc::Client>(blob_relay_info.endpoint(), blob_relay_info.blob_session_id());
    };
    std::function<std::string(ipc_client&, tateyama::proto::framework::response::Header::PayloadType&, blobs_type&)> request_ =
        [this](ipc_client& client, tateyama::proto::framework::response::Header::PayloadType& type, blobs_type& blobs){
            client.send(service_id_of_blob_relay_streaming_test_service, std::string(request_test_message), blobs);

            std::string res{};
            client.receive(res, type);

            return res;
        };
};

TEST_F(ipc_blob_relay_streaming_test, normal) {
    auto test_client = [this](ipc_client& client){
        handshake_(client);

        EXPECT_TRUE(grpc_client_);
        std::uint64_t object_id;
        std::uint64_t tag;
        grpc_client_->put(blob_size, object_id, tag);

        blobs_type blobs{};
        blobs.emplace(channel_name, object_id, tag, false);

        tateyama::proto::framework::response::Header::PayloadType type{};
        auto response = request_(client, type, blobs);
        EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
        EXPECT_EQ(response, response_test_message);
    };
    ipc_blob_relay_streaming_test_server_client sc{cfg_, test_client};
    sc.start_server_client();

    auto service = sc.service();
    auto& blobs = service->blobs();
    EXPECT_FALSE(blobs.empty());
    EXPECT_EQ(blobs.size(), 1);
    for (auto&& e: blobs) {
        auto blob_path = std::get<1>(e);
        EXPECT_TRUE(std::filesystem::exists(blob_path));
        EXPECT_EQ(std::filesystem::file_size(blob_path), blob_size);
        EXPECT_TRUE(grpc::Client::compare(blob_path));
        EXPECT_FALSE(std::get<2>(e));
    }
}

} // namespace tateyama::endpoint::ipc
