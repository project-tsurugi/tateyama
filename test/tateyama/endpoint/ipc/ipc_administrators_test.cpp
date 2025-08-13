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
#include <nlohmann/json.hpp>

#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

#include "tateyama/authentication/resource/authentication_adapter_test.h"
#include "tateyama/authentication/resource/crypto/key.h"
#include "crypto/rsa.h"

#include "ipc_client.h"
#include "ipc_gtest_base.h"

namespace tateyama::endpoint::ipc {

class post_administrators_service: public server_service_base {
public:
    explicit post_administrators_service(std::function<void(const tateyama::api::server::user_type type)> g) : g_(std::move(g)) {
    }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        g_(req->session_info().user_type());
        res->body(req->payload());
        return true;
    }

private:
    std::function<void(const tateyama::api::server::user_type type)> g_;
};

class ipc_administrators_test_server_client: public server_client_gtest_base {
public:
    ipc_administrators_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::function<void(ipc_client&)> f, std::function<void(const tateyama::api::server::user_type type)> g)
        : server_client_gtest_base(cfg), f_(std::move(f)), g_(std::move(g)) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<post_administrators_service>(g_);
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
    std::function<void(const tateyama::api::server::user_type type)> g_;
};

class ipc_administrators_test: public ipc_gtest_base {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
    bool called_{};
    tateyama::api::server::user_type type_{};
    tateyama::proto::endpoint::response::Handshake_Success::UserNameOptCase expected_case_;
    std::string expected_name_;

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
            EXPECT_EQ(expected_case_, response.success().user_name_opt_case());
            if (response.success().user_name_opt_case() == tateyama::proto::endpoint::response::Handshake_Success::kUserName) {
                EXPECT_EQ(expected_name_, response.success().user_name());
            }
            std::string dmy_message{"dummy message"};
            client.send(post_administrators_service::tag, dmy_message);
            client.receive(res);
        }
    };

    std::function<void(const tateyama::api::server::user_type type)> check_{
        [this](const tateyama::api::server::user_type type) {
            called_ = true;
            type_ = type;
        }
    };

    std::string get_json_text(const std::string& user, const std::string& password) {
        nlohmann::json j;
        std::stringstream ss;

        j["format_version"] = 1;
        j["user"] = user;
        j["password"] = password;

        ss << j;
        return ss.str();
    }
};

TEST_F(ipc_administrators_test, inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin,user\n");

    // Expectations must be set in advance, as clients are forked,
    expected_case_ = tateyama::proto::endpoint::response::Handshake_Success::kUserName;
    expected_name_ = "user";

    ipc_administrators_test_server_client sc { cfg_, client_, check_ };
    sc.start_server_client();

    EXPECT_TRUE(called_);
    EXPECT_EQ(tateyama::api::server::user_type::administrator, type_);
}

TEST_F(ipc_administrators_test, exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");

    expected_case_ = tateyama::proto::endpoint::response::Handshake_Success::kUserName;
    expected_name_ = "user";

    ipc_administrators_test_server_client sc { cfg_, client_, check_ };
    sc.start_server_client();

    EXPECT_TRUE(called_);
    EXPECT_EQ(tateyama::api::server::user_type::standard, type_);
}

TEST_F(ipc_administrators_test, asterisk) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=*\n");

    expected_case_ = tateyama::proto::endpoint::response::Handshake_Success::kUserName;
    expected_name_ = "user";

    ipc_administrators_test_server_client sc { cfg_, client_, check_ };
    sc.start_server_client();

    EXPECT_TRUE(called_);
    EXPECT_EQ(tateyama::api::server::user_type::administrator, type_);
}

TEST_F(ipc_administrators_test, no_auth) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=false\n");

    expected_case_ = tateyama::proto::endpoint::response::Handshake_Success::USER_NAME_OPT_NOT_SET;
    expected_name_ = "";

    ipc_administrators_test_server_client sc { cfg_, client_, check_ };
    sc.start_server_client();

    EXPECT_TRUE(called_);
    EXPECT_EQ(tateyama::api::server::user_type::administrator, type_);
}

} // namespace tateyama::endpoint::ipc
