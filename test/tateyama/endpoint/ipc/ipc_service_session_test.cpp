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

class synced_user_client : public ipc_client {
public:
    synced_user_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, boost::barrier& finish)
        : ipc_client(cfg, true), finish_(finish) {
    }
    void finish_wait() {
        finish_.wait();
    }

private:
    boost::barrier& finish_;
};
class synced_another_client : public ipc_client {
public:
    synced_another_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, boost::barrier& end, boost::barrier& finish)
        : ipc_client(cfg, true), end_(end), finish_(finish) {
    }
    void end_wait() {
        end_.wait();
    }
    void finish_wait() {
        finish_.wait();
    }

private:
    boost::barrier& end_;
    boost::barrier& finish_;
};

class ipc_service_session_test_server_client: public server_client_gtest_base {
public:
    ipc_service_session_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
                                           std::function<void(synced_user_client&)> f, std::function<void(synced_another_client&)> a, std::function<void(synced_another_client&)> u, std::function<void(synced_another_client&)> o)
        : server_client_gtest_base(cfg), f_(std::move(f)), a_(std::move(a)), u_(std::move(u)), o_(std::move(o)) {
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
        boost::barrier sync_admin{3};   // admin(2) + 1
        boost::barrier sync_another{3};  // another(2) + 1
        boost::barrier sync_user{2};    // user(1) + 1
        boost::barrier sync_finish{6};  // admin(2) + another(2) + user(1) + checker(1)

        std::thread threads[5];
        for (int i = 0; i < 2; i++ ) {
            threads[i] = std::thread([this, &sync_admin, &sync_finish]() {
                synced_another_client client { cfg_, sync_admin, sync_finish };  // client that skips handshake operation
                a_(client);
            });
        }
        sync_admin.wait();
        for (int i = 2; i < 4; i++ ) {
            threads[i] = std::thread([this, &sync_another, &sync_finish]() {
                synced_another_client client { cfg_, sync_another, sync_finish };  // client that skips handshake operation
                o_(client);
            });
        }
        sync_another.wait();
        for (int i = 4; i < 5; i++ ) {
            threads[i] = std::thread([this, &sync_user, &sync_finish]() {
                synced_another_client client { cfg_, sync_user, sync_finish };  // client that skips handshake operation
                u_(client);
            });
        }

        sync_user.wait();
        synced_user_client client { cfg_, sync_finish };  // client that skips handshake operation
        f_(client);

        for (int i = 0; i < 5; i++ ) {
            threads[i].join();
        }
    }

private:
    std::function<void(synced_user_client&)> f_;
    std::function<void(synced_another_client&)> a_;
    std::function<void(synced_another_client&)> u_;
    std::function<void(synced_another_client&)> o_;
};

class ipc_service_session_test: public ipc_gtest_base {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
    std::size_t expected_entry_size_{};
    tateyama::proto::framework::response::Header::PayloadType expected_type_{};

    void handshake(ipc_client& client, const std::string user, const std::string password) {
            crypto::rsa_encrypter rsa{crypto::base64_decode(std::string(tateyama::authentication::resource::crypto::public_key))};

            std::string c{};
            rsa.encrypt(get_json_text(user, password), c);
            std::string encrypted_credential = crypto::base64_encode(c);

            tateyama::proto::endpoint::request::Request endpoint_request{};
            tateyama::proto::endpoint::request::Handshake* endpoint_handshake = endpoint_request.mutable_handshake();
            tateyama::proto::endpoint::request::ClientInformation* clientInformation = endpoint_handshake->mutable_client_information();
            tateyama::proto::endpoint::request::Credential* credential = clientInformation->mutable_credential();
            credential->set_encrypted_credential(encrypted_credential);

            tateyama::proto::endpoint::request::WireInformation* wire_information = endpoint_handshake->mutable_wire_information();
            (void) wire_information->mutable_ipc_information();

            client.send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
            tateyama::proto::endpoint::response::Handshake response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            EXPECT_EQ(response.result_case(), tateyama::proto::endpoint::response::Handshake::kSuccess);
    }

    // Works on forked process.
    std::function<void(synced_user_client&)> client_list_{
        [this](synced_user_client& client){
            handshake(client, "user", "pass");

            tateyama::proto::session::request::Request request{};
            (void) request.mutable_session_list();
            client.send(tateyama::framework::service_id_session, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);

            tateyama::proto::session::response::SessionList response{};
            if(!response.ParseFromString(res)) {
                FAIL();
            }
            if(response.result_case() != tateyama::proto::session::response::SessionList::kSuccess) {
                FAIL();
            }
            EXPECT_EQ(response.success().entries_size(), expected_entry_size_);
            client.finish_wait();
        }
    };
    std::function<void(synced_user_client&)> client_get_{
        [this](synced_user_client& client){
            handshake(client, "user", "pass");

            tateyama::proto::session::request::Request request{};
            auto* get = request.mutable_session_get();
            get->set_session_specifier(":1");
            client.send(tateyama::framework::service_id_session, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(expected_type_, type);
            client.finish_wait();
        }
    };
    std::function<void(synced_user_client&)> client_shutdown_{
        [this](synced_user_client& client){
            handshake(client, "user", "pass");

            tateyama::proto::session::request::Request request{};
            auto* shutdown = request.mutable_session_shutdown();
            shutdown->set_session_specifier(":1");
            shutdown->set_request_type(tateyama::proto::session::request::SessionShutdownType::GRACEFUL);
            client.send(tateyama::framework::service_id_session, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(expected_type_, type);
            client.finish_wait();
        }
    };
    std::function<void(synced_user_client&)> client_set_variable_{
        [this](synced_user_client& client){
            handshake(client, "user", "pass");

            tateyama::proto::session::request::Request request{};
            auto* set_variable = request.mutable_session_set_variable();
            set_variable->set_session_specifier(":1");
            set_variable->set_name("dummy_name");
            set_variable->set_value("dummy_value");
            client.send(tateyama::framework::service_id_session, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(expected_type_, type);
            client.finish_wait();
        }
    };
    std::function<void(synced_user_client&)> client_get_variable_{
        [this](synced_user_client& client){
            handshake(client, "user", "pass");

            tateyama::proto::session::request::Request request{};
            auto* get_variable = request.mutable_session_get_variable();
            get_variable->set_session_specifier(":1");
            get_variable->set_name("dummy_name");
            client.send(tateyama::framework::service_id_session, request.SerializeAsString());

            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client.receive(res, type);
            EXPECT_EQ(expected_type_, type);
            client.finish_wait();
        }
    };
    std::function<void(synced_another_client&)> client_admin_{
        [this](synced_another_client& client){
            handshake(client, "admin", "test");
            client.end_wait();

            client.finish_wait();
        }
    };
    std::function<void(synced_another_client&)> client_another_{
        [this](synced_another_client& client){
            handshake(client, "another", "pass");
            client.end_wait();

            client.finish_wait();
        }
    };
    std::function<void(synced_another_client&)> client_user_{
        [this](synced_another_client& client){
            handshake(client, "user", "pass");
            client.end_wait();

            client.finish_wait();
        }
    };
};

TEST_F(ipc_service_session_test, list_inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin,user\n");
    expected_entry_size_ = 5;

    ipc_service_session_test_server_client sc { cfg_, client_list_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, list_exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");
    expected_entry_size_ = 1;

    ipc_service_session_test_server_client sc { cfg_, client_list_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, get_inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin, user\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVICE_RESULT;

    ipc_service_session_test_server_client sc { cfg_, client_get_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, get_exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    ipc_service_session_test_server_client sc { cfg_, client_get_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, shutdown_inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin, user\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVICE_RESULT;

    ipc_service_session_test_server_client sc { cfg_, client_shutdown_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, shutdown_exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    ipc_service_session_test_server_client sc { cfg_, client_shutdown_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, set_variable_inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin, user\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVICE_RESULT;

    ipc_service_session_test_server_client sc { cfg_, client_set_variable_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, set_variable_exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    ipc_service_session_test_server_client sc { cfg_, client_set_variable_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, get_variable_inclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin, user\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVICE_RESULT;

    ipc_service_session_test_server_client sc { cfg_, client_get_variable_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

TEST_F(ipc_service_session_test, get_variable_exclusive) {
    ipc_test_env::setup("[authentication]\n"
                        "  enabled=true\n"
                        "  administrators=admin\n");
    expected_type_ = tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS;

    ipc_service_session_test_server_client sc { cfg_, client_get_variable_, client_admin_, client_user_, client_another_ };
    sc.start_server_client();
}

} // namespace tateyama::endpoint::ipc
