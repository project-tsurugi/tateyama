/*
 * Copyright 2018-2021 tsurugi project.
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
#include "ipc_client.h"

namespace tateyama::api::endpoint::ipc {

class session_limit_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(payload));
        return true;
    }
};

class ipc_session_limit_test_server_client: public server_client_base {
public:
    ipc_session_limit_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nclient,
            int nthread, std::size_t msg_len, int nsession, bool pararell, int nloop) :
            server_client_base(cfg, nclient, nthread), msg_len_(msg_len), nsession_(nsession), pararell_(pararell), nloop_(
                    nloop) {
        get_ipc_max_session(cfg, ipc_max_session_);
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<session_limit_service>();
    }

    void server() override {
        server_client_base::server();
        //
        std::cout << "msg_len=" << msg_len_;
        std::cout << ", nsession_all=" << nsession_ * nworker_;
        std::cout << ", nsession/client=" << nsession_;
        std::cout << ", pararell=" << (pararell_ ? "true" : "false");
        std::cout << ", nloop=" << nloop_ << ", ";
        std::size_t msg_num = nloop_ * 2 * nworker_ * nsession_;
        std::size_t len_sum = msg_num * msg_len_;
        server_client_base::server_dump(msg_num, len_sum);
    }

    void send_receive_loop(ipc_client &client) {
        for (int i = 0; i < nloop_; i++) {
            std::string req_message;
            make_dummy_message(client.session_id(), msg_len_, req_message);
            client.send(session_limit_service::tag, req_message);
            std::string res_message;
            client.receive(res_message);
            EXPECT_EQ(req_message, res_message);
        }
    }

    void client_thread_serial() {
        std::vector<ipc_client> clients { };
        for (int i = 1; i <= nsession_; i++) {
            try {
                clients.emplace_back(cfg_);
            } catch (std::exception &ex) {
                std::cout << "nclient=" << i << ", ipc_max_session=" << ipc_max_session_ << " : " << ex.what()
                        << std::endl;
                if (i <= ipc_max_session_) {
                    // shouldn't be failed
                    FAIL();
                } else {
                    // should be failed
                    EXPECT_TRUE(true);
                    return;
                }
            }
        }
        for (ipc_client &client : clients) {
            send_receive_loop(client);
        }
    }

    void client_thread_pararell() {
        std::vector<std::thread> threads { };
        threads.reserve(nsession_);
        for (int i = 0; i < nsession_; i++) {
            threads.emplace_back([this] {
                ipc_client client { cfg_ };
                send_receive_loop(client);
            });
        }
        for (std::thread &th : threads) {
            if (th.joinable()) {
                th.join();
            }
        }
    }

    void client_thread() override {
        if (pararell_) {
            client_thread_pararell();
        } else {
            client_thread_serial();
        }
    }
private:
    int ipc_max_session_;
    std::size_t msg_len_;
    int nsession_ { };
    bool pararell_ { };
    int nloop_ { };
};

class ipc_session_limit_test: public ipc_test_base {
};

TEST_F(ipc_session_limit_test, max) {
    std::cout << "ipc_max_session: " << ipc_max_session_ << std::endl;
    std::vector<std::size_t> msg_len_list { 32 * 1024 };
    std::vector<int> nsession_list { 32, 64, ipc_max_session_ };
    std::vector<int> nsession_par_client_list { 0, 4, 8 };
    std::vector<bool> pararell_list { false, true };
    for (int nsession : nsession_list) {
        for (int nsession_par_client : nsession_par_client_list) {
            for (std::size_t msg_len : msg_len_list) {
                for (bool pararell : pararell_list) {
                    int nclient, nses_client;
                    if (nsession_par_client > 0) {
                        nclient = nsession / nsession_par_client;
                        nses_client = nsession_par_client;
                    } else {
                        nclient = 1;
                        nses_client = nsession;
                    }
                    ipc_session_limit_test_server_client sc { cfg_, nclient, 0, msg_len, nses_client, pararell, 10 };
                    sc.start_server_client();
                }
            }
        }
    }
}

TEST_F(ipc_session_limit_test, max_plus_1_sessions) {
    ipc_session_limit_test_server_client sc { cfg_, 1, 0, 128, ipc_max_session_ + 1, false, 1 };
    sc.start_server_client();
}

} // namespace tateyama::api::endpoint::ipc
