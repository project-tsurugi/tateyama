/*
 * Copyright 2018-2023 tsurugi project.
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
#include "tateyama/endpoint/ipc/ipc_test_env.h"
#include "tateyama/endpoint/ipc/ipc_client.h"
#include "tateyama/endpoint/ipc/server_client_base.h"

using namespace tateyama::api::endpoint::ipc;

class echo_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        res->session_id(req->session_id());
        res->body(req->payload());
        return true;
    }
};

class reqres_sync_loop_server_client: public server_client_base {
public:
    reqres_sync_loop_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nclient,
            int nthread, std::size_t msg_len, int nloop) :
            server_client_base(cfg, nclient, nthread), msg_len_(msg_len), nloop_(nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<echo_service>();
    }

    static void result_header() {
        std::cout << "# nsession, single_client_proc, msg_len, elapse_sec, msg_num/sec, MB/sec" << std::endl;
    }

    void server() override {
        server_client_base::server();
        //
        std::cout << nworker_;
        std::cout << "," << (nthread_ == 0 ? "false" : "true");
        std::cout << "," << msg_len_;
        //
        std::int64_t msec = server_elapse_.msec();
        std::size_t msg_num = nloop_ * 2;
        std::size_t len_sum = nloop_ * 2 * msg_len_;
        double sec = msec / 1000.0;
        double mb_len = len_sum / (1024.0 * 1024.0);
        std::cout << "," << std::fixed << std::setprecision(3) << sec;
        std::cout << "," << std::fixed << std::setprecision(1) << msg_num / sec;
        std::cout << "," << std::fixed << std::setprecision(1) << mb_len / sec;
        std::cout << std::endl;
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        for (int i = 0; i < nloop_; i++) {
            std::string req_message;
            req_message.reserve(msg_len_);
            client.send(echo_service::tag, req_message);
            std::string res_message;
            client.receive(res_message);
        }
    }

private:
    std::size_t msg_len_;
    int nloop_ { };
};

int main(int argc, char **argv) {
    ipc_test_env env;
    env.setup();
    //
    std::vector<int> nsession_list { 1, 2, 4, 8, 16, 32, 64, env.ipc_max_session() };
    std::vector<std::size_t> msg_len_list { 128, 256, 512, 1024, 4 * 1024, 32 * 1024 };
    std::vector<bool> single_client_list { true, false };
    const int nloop = 10000;
    //
    reqres_sync_loop_server_client::result_header();
    for (int nsession : nsession_list) {
        for (std::size_t msg_len : msg_len_list) {
            for (bool single_client : single_client_list) {
                int nclient = (single_client ? 1 : nsession);
                int nthread = (single_client ? nsession : 0);
                reqres_sync_loop_server_client sc { env.config(), nclient, nthread, msg_len, nloop };
                sc.start_server_client();
            }
        }
    }
    //
    env.teardown();
}
