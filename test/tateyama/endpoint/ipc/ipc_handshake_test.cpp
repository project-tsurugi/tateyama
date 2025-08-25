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
#include <sstream>
#include <chrono>

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

#include "tateyama/authentication/resource/authentication_adapter_test.h"
#include "tateyama/authentication/resource/crypto/key.h"
#include "crypto/rsa.h"
#include "jwt/token_creator.h"

#include "ipc_client.h"
#include "ipc_gtest_base.h"

namespace tateyama::endpoint::ipc {

class post_handshake_service: public server_service_base {
public:
    explicit post_handshake_service(std::function<void(const std::string)> g) : g_(std::move(g)) {
    }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        if (auto name_opt = req->session_info().username(); name_opt) {
            user_name_ = name_opt.value();
        }
        res->body(req->payload());
        g_(user_name_);
        return true;
    }

private:
    std::function<void(const std::string)> g_;
    std::string user_name_;
};

class ipc_handshake_test_server_client: public server_client_gtest_base {
public:
    ipc_handshake_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::function<void(ipc_client&)> f, std::function<void(std::string)> g) : server_client_gtest_base(cfg), f_(std::move(f)), g_(std::move(g))  {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<post_handshake_service>(g_);
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
        ipc_client client { cfg_, true };  // client that skips handshake operation
        f_(client);
    }

private:
    std::function<void(ipc_client&)> f_;
    std::function<void(const std::string)> g_;
};

class ipc_handshake_test: public ipc_gtest_base {
    void SetUp() override {
        handler_ = std::make_unique<jwt::token_creator>(crypto::base64_decode(std::string(tateyama::authentication::resource::crypto::private_key)));
        ipc_test_env::setup("[authentication]\n"
                            "  enabled=true\n");
    }

    void TearDown() override {
    }

protected:
    crypto::rsa_encrypter rsa_{crypto::base64_decode(std::string(tateyama::authentication::resource::crypto::public_key))};
    std::unique_ptr<jwt::token_creator> handler_{};
};

TEST_F(ipc_handshake_test, encryption_key) {
    ipc_handshake_test_server_client sc { cfg_,
        [](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            request.mutable_encryption_key();
            client.send(tateyama::framework::service_id_endpoint_broker, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::EncryptionKey response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::EncryptionKey::kSuccess);
            std::string key = response.success().encryption_key();
            EXPECT_EQ(key.find("-----BEGIN PUBLIC KEY-----"), 0);
        },
        [](const std::string){}
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_test, user_pass) {
    ipc_handshake_test_server_client sc { cfg_,
        [this](ipc_client& client){
            std::string c{};
            rsa_.encrypt(get_json_text("user", "pass"), c);
            std::string encrypted_credential = crypto::base64_encode(c);

            tateyama::proto::endpoint::request::Request request{};
            auto* endpoint_handshake = request.mutable_handshake();
            auto* client_information = endpoint_handshake->mutable_client_information();
            auto* credential = client_information->mutable_credential();
            credential->set_encrypted_credential(encrypted_credential);
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
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::EncryptionKey::kSuccess);

            std::string dmy_message{"dummy message"};
            client.send(post_handshake_service::tag, dmy_message);
            client.receive(res);
        },
        [](const std::string user_name) {
            EXPECT_EQ(user_name, "user");
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_test, user_wrong_pass) {
    ipc_handshake_test_server_client sc { cfg_,
        [this](ipc_client& client){
            std::string c{};
            rsa_.encrypt(get_json_text("user", "ssap"), c);
            std::string encrypted_credential = crypto::base64_encode(c);

            tateyama::proto::endpoint::request::Request request{};
            auto* endpoint_handshake = request.mutable_handshake();
            auto* client_information = endpoint_handshake->mutable_client_information();
            auto* credential = client_information->mutable_credential();
            credential->set_encrypted_credential(encrypted_credential);
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
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::EncryptionKey::kError);
        },
        [](const std::string user_name) {}
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_test, token_valid) {
    auto ns = std::chrono::system_clock::now().time_since_epoch();
    std::string token{};
    if (auto token_opt = handler_->create_token("user", std::chrono::duration_cast<std::chrono::seconds>(ns).count() + 10); token_opt) {
        token = token_opt.value();
    } else {
        FAIL();
    }

    ipc_handshake_test_server_client sc { cfg_,
        [token](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            auto* endpoint_handshake = request.mutable_handshake();
            auto* client_information = endpoint_handshake->mutable_client_information();
            auto* credential = client_information->mutable_credential();
            credential->set_remember_me_credential(token);
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

            std::string dmy_message{"dummy message"};
            client.send(post_handshake_service::tag, dmy_message);
            client.receive(res);
        },
        [](const std::string user_name) {
            EXPECT_EQ(user_name, "user");
        }
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_test, token_expired) {
    auto ns = std::chrono::system_clock::now().time_since_epoch();
    std::string token{};
    if (auto token_opt = handler_->create_token("user", std::chrono::duration_cast<std::chrono::seconds>(ns).count() - 10); token_opt) {
        token = token_opt.value();
    } else {
        FAIL();
    }

    ipc_handshake_test_server_client sc { cfg_,
        [token](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            auto* endpoint_handshake = request.mutable_handshake();
            auto* client_information = endpoint_handshake->mutable_client_information();
            auto* credential = client_information->mutable_credential();
            credential->set_remember_me_credential(token);
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
        },
        [](const std::string){}
    };
    sc.start_server_client();
}

TEST_F(ipc_handshake_test, token_invalid) {
    auto ns = std::chrono::system_clock::now().time_since_epoch();
    std::string token{};
    if (auto token_opt = handler_->create_token("user", std::chrono::duration_cast<std::chrono::seconds>(ns).count() + 10); token_opt) {
        token = token_opt.value();
    } else {
        FAIL();
    }
    token.replace(10, 1, "\n");

    ipc_handshake_test_server_client sc { cfg_,
        [token](ipc_client& client){
            tateyama::proto::endpoint::request::Request request{};
            auto* endpoint_handshake = request.mutable_handshake();
            auto* client_information = endpoint_handshake->mutable_client_information();
            auto* credential = client_information->mutable_credential();
            credential->set_remember_me_credential(token);
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
        },
        [](const std::string){}
    };
    sc.start_server_client();
}

} // namespace tateyama::endpoint::ipc
