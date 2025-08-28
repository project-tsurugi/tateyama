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

#include "tateyama/authentication/resource/authentication_adapter_test.h"
#include "tateyama/authentication/resource/crypto/key.h"
#include "crypto/rsa.h"

#include "ipc_client.h"
#include "ipc_gtest_base.h"

namespace tateyama::endpoint::ipc {


class ipc_authentication_expiration_test_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        return true;
    }
};

class ipc_authentication_expiration_test_server_client: public server_client_gtest_base {
public:
    ipc_authentication_expiration_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::function<void(ipc_client&)> f)
        : server_client_gtest_base(cfg), f_(std::move(f)) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<ipc_authentication_expiration_test_service>();
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

class ipc_authentication_expiration_test: public ipc_gtest_base {
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(ipc_authentication_expiration_test, normal) {
    ipc_test_env::setup("[session]\n"
                        "authentication_timeout=600\n"
                        "[authentication]\n"
                        "enabled=true\n"
                        "administrators=*\n");

    ipc_authentication_expiration_test_server_client sc {
        cfg_,
        [this](ipc_client& client){
            {
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
                // finish handshake
            }
            {
                tateyama::proto::endpoint::request::GetAuthenticationExpirationTime get_authentication_expiration_time{};
                tateyama::proto::endpoint::request::Request endpoint_request{};
                endpoint_request.set_allocated_get_authentication_expiration_time(&get_authentication_expiration_time);
                client.send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());
                (void)endpoint_request.release_get_authentication_expiration_time();

                std::string res{};
                tateyama::proto::framework::response::Header::PayloadType type{};
                client.receive(res, type);
                EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
                tateyama::proto::endpoint::response::GetAuthenticationExpirationTime response{};
                if(!response.ParseFromString(res)) {
                    FAIL();
                }

                EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::GetAuthenticationExpirationTime::kSuccess);
                auto expiration_time_returned = response.success().expiration_time();

                auto ee = tateyama::session::session_context::expiration_time_type::clock::now() + std::chrono::seconds(600);
                auto expiration_time_expected = std::chrono::duration_cast<std::chrono::milliseconds>(ee.time_since_epoch()).count();
                EXPECT_LE(expiration_time_returned, expiration_time_expected);

                auto ue = tateyama::session::session_context::expiration_time_type::clock::now() + std::chrono::seconds(600 - 5);
                auto expiration_time_under = std::chrono::duration_cast<std::chrono::milliseconds>(ue.time_since_epoch()).count();
                EXPECT_GT(expiration_time_returned, expiration_time_under);
            }
        }
    };

    sc.start_server_client();
}

} // namespace tateyama::endpoint::ipc
