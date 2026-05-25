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
#include <sstream>
#include <chrono>

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

#include "ipc_client.h"
#include "ipc_gtest_base.h"

namespace tateyama::endpoint::ipc {

class ipc_handshake_blob_test_server_client: public server_client_gtest_base {
public:
    ipc_handshake_blob_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::function<void(ipc_client&)> f) : server_client_gtest_base(cfg), f_(std::move(f)) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() {
        return nullptr;
    }

    void server() override {
        server_client_base::server([](tateyama::framework::server& sv, std::shared_ptr<tateyama::api::configuration::whole> const & cfg){}, 'f');
    }

    void client_thread() override {
        ipc_client client { cfg_, true };  // client that skips handshake operation
        f_(client);
    }

private:
    std::function<void(ipc_client&)> f_;
};

class ipc_handshake_blob_test: public ipc_gtest_base {
protected:
    void prepare(const std::string& config) {
        ipc_test_env::setup(config);
    }

    void prepare() {
        prepare(
            "[ipc_endpoint]\n"
            "  allow_blob_privileged=true\n"
            "[grpc_server]\n"
            "  enabled=true\n"
            "  listen_address=0.0.0.0:62345\n"
            "  endpoint=dns:///localhost:62345\n"
            "  secure=false\n"
            "[blob_relay]\n"
            "  enabled=true\n"
        );
    }

    tateyama::proto::endpoint::request::Request endpoint_request() {
        tateyama::proto::endpoint::request::Request request{};
        request.set_service_message_version_major(0);
        request.set_service_message_version_minor(2);
        return request;
    }

    bool gate() {
        return pid_ != 0;
    }

private:
    void SetUp() override {
        pid_ = fork();
        if (pid_ > 0) {
            int status = 0;
            if (waitpid(pid_, &status, 0) < 0) {
                FAIL();
            }
        }
    }

    void TearDown() override {
        if (pid_ == 0) {
            exit(0);
        }
    }

    pid_t pid_;
};

TEST_F(ipc_handshake_blob_test, blob_does_not_use) {
    if (gate()) {
        return;
    }
    prepare();
    ipc_handshake_blob_test_server_client sc { cfg_,
        [this](ipc_client& client){
            auto request = endpoint_request();
            auto* endpoint_handshake = request.mutable_handshake();
            auto* blob_transfer_media = endpoint_handshake->add_blob_transfer_media();
            blob_transfer_media->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::DOES_NOT_USE);
            auto* wire_information = endpoint_handshake->mutable_wire_information();
            wire_information->mutable_ipc_information();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
            auto& success = response.success();
            EXPECT_EQ(success.blob_transfer_case(), tateyama::proto::endpoint::response::Handshake_Success::BlobTransferCase::BLOB_TRANSFER_NOT_SET);
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_blob_test, blob_relay_privileged) {
    if (gate()) {
        return;
    }
    prepare();
    ipc_handshake_blob_test_server_client sc { cfg_,
        [this](ipc_client& client){
            auto request = endpoint_request();
            auto* endpoint_handshake = request.mutable_handshake();
            auto* blob_transfer_media = endpoint_handshake->add_blob_transfer_media();
            blob_transfer_media->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::PRIVILEGED);
            auto* wire_information = endpoint_handshake->mutable_wire_information();
            wire_information->mutable_ipc_information();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
            auto& success = response.success();
            EXPECT_EQ(success.blob_transfer_case(), tateyama::proto::endpoint::response::Handshake_Success::BlobTransferCase::kPrivilegedMode);
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_blob_test, blob_relay_streaming) {
    if (gate()) {
        return;
    }
    prepare();
    ipc_handshake_blob_test_server_client sc { cfg_,
        [this](ipc_client& client){
            auto request = endpoint_request();
            auto* endpoint_handshake = request.mutable_handshake();
            auto* blob_transfer_media = endpoint_handshake->add_blob_transfer_media();
            blob_transfer_media->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::RELAY);
            auto* wire_information = endpoint_handshake->mutable_wire_information();
            wire_information->mutable_ipc_information();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
            auto& success = response.success();
            EXPECT_EQ(success.blob_transfer_case(), tateyama::proto::endpoint::response::Handshake_Success::BlobTransferCase::kBlobRelayServiceInfo);
            auto& blob_relay_service_info = success.blob_relay_service_info();
            EXPECT_EQ(blob_relay_service_info.endpoint(), "dns:///localhost:62345");
            EXPECT_EQ(blob_relay_service_info.medium(), "streaming");
            auto& parameters = blob_relay_service_info.parameters();
            EXPECT_TRUE(parameters.find("secure") == parameters.end());
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_blob_test, blob_does_not_use_01) {
    if (gate()) {
        return;
    }
    prepare();
    ipc_handshake_blob_test_server_client sc { cfg_,
        [this](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            request.set_service_message_version_major(0);
            request.set_service_message_version_minor(1);
            auto* endpoint_handshake = request.mutable_handshake();
            auto* blob_transfer_media = endpoint_handshake->add_blob_transfer_media();
            blob_transfer_media->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::DOES_NOT_USE);
            auto* wire_information = endpoint_handshake->mutable_wire_information();
            wire_information->mutable_ipc_information();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
            auto& success = response.success();
            EXPECT_EQ(success.blob_transfer_case(), tateyama::proto::endpoint::response::Handshake_Success::BlobTransferCase::kPrivilegedMode);
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_blob_test, blob_relay_streaming_01) {
    if (gate()) {
        return;
    }
    prepare();
    ipc_handshake_blob_test_server_client sc { cfg_,
        [this](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            request.set_service_message_version_major(0);
            request.set_service_message_version_minor(1);
            auto* endpoint_handshake = request.mutable_handshake();
            auto* blob_transfer_media = endpoint_handshake->add_blob_transfer_media();
            blob_transfer_media->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::RELAY);
            auto* wire_information = endpoint_handshake->mutable_wire_information();
            wire_information->mutable_ipc_information();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
            auto& success = response.success();
            EXPECT_EQ(success.blob_transfer_case(), tateyama::proto::endpoint::response::Handshake_Success::BlobTransferCase::kPrivilegedMode);
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_blob_test, blob_does_not_use_03) {
    if (gate()) {
        return;
    }
    prepare();
    ipc_handshake_blob_test_server_client sc { cfg_,
        [this](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            request.set_service_message_version_major(0);
            request.set_service_message_version_minor(3);
            auto* endpoint_handshake = request.mutable_handshake();
            auto* blob_transfer_media = endpoint_handshake->add_blob_transfer_media();
            blob_transfer_media->set_blob_transfer_type(tateyama::proto::endpoint::request::BlobTransferType::DOES_NOT_USE);
            auto* wire_information = endpoint_handshake->mutable_wire_information();
            wire_information->mutable_ipc_information();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kError);
            auto& error = response.error();
            EXPECT_EQ(error.code(), tateyama::proto::diagnostics::Code::INVALID_REQUEST);
        }
    };
    sc.start_server_client();
}

} // namespace tateyama::endpoint::ipc
