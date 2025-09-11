/*
 * Copyright 2018-2025 Project Tsurugi.
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
#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/metrics/request.pb.h>
#include <tateyama/proto/request/request.pb.h>
#ifdef ENABLE_ALTIMETER
#include <tateyama/proto/altimeter/request.pb.h>
#endif

#include "tateyama/authentication/resource/authentication_adapter_test.h"
#include "tateyama/authentication/resource/crypto/key.h"
#include "crypto/rsa.h"

#include "ipc_client.h"
#include "ipc_gtest_base.h"

namespace tateyama::endpoint::ipc {

class null_service: public server_service_base {
public:
    explicit null_service() {
    }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        return true;
    }
};

class ipc_service_admin_test_server_client: public server_client_gtest_base {
public:
    ipc_service_admin_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::function<void(ipc_client&)> f)
        : server_client_gtest_base(cfg), f_(std::move(f)) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<null_service>();
    }

    void server() override {
        server_client_base::server(
            [](tateyama::framework::server& sv, std::shared_ptr<tateyama::api::configuration::whole> const & cfg) {
                if (cfg->get_section("authentication")->get<bool>("enabled").value()) {
                    auto auth = sv.find_resource<tateyama::authentication::resource::bridge>();
                    if (auth) {
                        auth->preset_authentication_adapter(std::make_unique<tateyama::authentication::resource::authentication_adapter_test>(true));
                    }
                }
            }
        );
    }

    void client_thread() override {
        ipc_client client { cfg_, true };  // client that skips administrators operation
        f_(client);
    }

private:
    std::function<void(ipc_client&)> f_;
};

class ipc_service_admin_test: public ipc_gtest_base {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
    tateyama::proto::framework::response::Header::PayloadType expected_type_;
    std::function<void(ipc_client&)> invoke_resource_;

    // Works on forked process.
    std::function<void(ipc_client&)> client_{
        [this](ipc_client& client){
            crypto::rsa_encrypter rsa{crypto::base64_decode(std::string(tateyama::authentication::resource::crypto::public_key))};

            std::string c{};
            rsa.encrypt(get_json_text("user", "pass"), c);
            std::string encrypted_credential = crypto::base64_encode(c);

            tateyama::proto::endpoint::request::Handshake endpoint_handshake{};
            tateyama::proto::endpoint::request::ClientInformation client_information{};
            tateyama::proto::endpoint::request::Credential credential{};
            credential.set_encrypted_credential(encrypted_credential);
            client_information.set_allocated_credential(&credential);
            endpoint_handshake.set_allocated_client_information(&client_information);

            tateyama::proto::endpoint::request::WireInformation wire_information{};
            wire_information.mutable_ipc_information();
            endpoint_handshake.set_allocated_wire_information(&wire_information);

            tateyama::proto::endpoint::request::Request endpoint_request{};
            endpoint_request.set_allocated_handshake(&endpoint_handshake);
            client.send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());
            (void) endpoint_request.release_handshake();
            (void) endpoint_handshake.release_wire_information();
            (void) endpoint_handshake.release_client_information();
            (void) client_information.release_credential();

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);

            invoke_resource_(client);
        }
    };
    std::function<void(ipc_client&)> session_{
        [this](ipc_client& client){
            tateyama::proto::session::request::Request request{};
            (void) request.mutable_session_list();
            client.send(tateyama::framework::service_id_session, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);

            EXPECT_EQ(type, expected_type_);
        }
    };
};

TEST_F(ipc_service_admin_test, inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin,user\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVICE_RESULT;
    
    invoke_resource_ = session_;
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}

TEST_F(ipc_service_admin_test, exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    invoke_resource_ = session_;
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}

TEST_F(ipc_service_admin_test, no_auth) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=false\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVICE_RESULT;

    invoke_resource_ = session_;
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}

TEST_F(ipc_service_admin_test, metrics_permission_error) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    invoke_resource_ = [this](ipc_client& client){
        tateyama::proto::metrics::request::Request request{};
        (void) request.mutable_list();
        client.send(tateyama::framework::service_id_metrics, request.SerializeAsString());

        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client.receive(res, type);

        EXPECT_EQ(type, expected_type_);
    };
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}

TEST_F(ipc_service_admin_test, datastore_permission_error) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    invoke_resource_ = [this](ipc_client& client){
        tateyama::proto::datastore::request::Request request{};
        (void) request.mutable_backup_begin();
        client.send(tateyama::framework::service_id_datastore, request.SerializeAsString());

        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client.receive(res, type);

        EXPECT_EQ(type, expected_type_);
    };
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}

TEST_F(ipc_service_admin_test, request_permission_error) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    invoke_resource_ = [this](ipc_client& client){
        tateyama::proto::request::request::Request request{};
        (void) request.mutable_list_request();
        client.send(tateyama::framework::service_id_request, request.SerializeAsString());

        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client.receive(res, type);

        EXPECT_EQ(type, expected_type_);
    };
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}

#ifdef ENABLE_ALTIMETER
TEST_F(ipc_service_admin_test, altimeter_permission_error) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");

    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    invoke_resource_ = [this](ipc_client& client){
        tateyama::proto::altimeter::request::Request request{};
        (void) request.mutable_configure();
        client.send(tateyama::framework::service_id_altimeter, request.SerializeAsString());

        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client.receive(res, type);

        EXPECT_EQ(type, expected_type_);
    };
    ipc_service_admin_test_server_client sc { cfg_, client_ };
    sc.start_server_client();
}
#endif

} // namespace tateyama::endpoint::ipc
