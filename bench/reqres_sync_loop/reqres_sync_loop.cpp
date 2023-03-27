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
        std::cout << "# session_num, multi_thread_or_procs, msg_len, nloop, elapse_sec, msg_num/sec, MB/sec"
                << std::endl;
    }

    void server() override {
        server_client_base::server();
        //
        std::cout << nworker_;
        std::cout << "," << (nthread_ > 0 ? "mt" : "mp"); // NOLINT
        std::cout << "," << msg_len_;
        std::cout << "," << nloop_;
        //
        std::int64_t msec = server_elapse_.msec();
        std::size_t msg_num = nloop_ * 2 * nworker_;
        std::size_t len_sum = msg_num * msg_len_;
        double sec = msec / 1000.0;
        double mb_len = len_sum / (1024.0 * 1024.0);
        msg_num_per_sec_ = msg_num / sec;
        std::cout << "," << std::fixed << std::setprecision(3) << sec;
        std::cout << "," << std::fixed << std::setprecision(1) << msg_num_per_sec_;
        std::cout << "," << std::fixed << std::setprecision(1) << mb_len / sec;
        std::cout << std::endl;
    }

    double msg_num_per_sec() {
        return msg_num_per_sec_;
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string req_message;
        make_dummy_message(gettid(), msg_len_, req_message);
        std::string res_message;
        for (int i = 0; i < nloop_; i++) {
            client.send(echo_service::tag, req_message);
            client.receive(res_message);
        }
    }

private:
    std::size_t msg_len_;
    int nloop_ { };
    double msg_num_per_sec_ { };
};

static std::string key(bool use_multi_thread, int nsession, std::size_t msg_len) {
    return std::to_string(nsession) + (use_multi_thread > 0 ? "mt" : "mp") + std::to_string(msg_len);
}

static void show_result_summary(std::vector<bool> use_multi_thread_list, std::vector<int> nsession_list,
        std::vector<std::size_t> msg_len_list, std::map<std::string, double> results) {
    for (bool use_multi_thread : use_multi_thread_list) {
        std::cout << std::endl;
        std::cout << "# summary : " << (use_multi_thread > 0 ? "multi_thread" : "multi_process") << " [msg/sec]"
                << std::endl;
        std::cout << "# nsession \\ msg_len";
        for (std::size_t msg_len : msg_len_list) {
            std::cout << ", " << msg_len;
        }
        std::cout << std::endl;
        //
        for (int nsession : nsession_list) {
            std::cout << nsession;
            for (std::size_t msg_len : msg_len_list) {
                double r = results[key(use_multi_thread, nsession, msg_len)];
                std::cout << "," << std::fixed << std::setprecision(1) << r;
            }
            std::cout << std::endl;
        }
    }
}

int main(int argc, char **argv) {
    ipc_test_env env;
    env.setup();
    //
    std::vector<int> nsession_list { 1, 2, 4, 8 };
    for (int n = 16; n <= env.ipc_max_session(); n += 8) {
        nsession_list.push_back(n);
    }
    std::vector<std::size_t> msg_len_list { 0, 128, 256, 512, 1024, 4 * 1024, 32 * 1024 };
    std::vector<bool> use_multi_thread_list { true, false };
    const int nloop = 100'000;
    //
    std::map<std::string, double> results { };
    reqres_sync_loop_server_client::result_header();
    for (bool use_multi_thread : use_multi_thread_list) {
        for (int nsession : nsession_list) {
            int nclient = (use_multi_thread ? 1 : nsession);
            int nthread = (use_multi_thread ? nsession : 0);
            for (std::size_t msg_len : msg_len_list) {
                reqres_sync_loop_server_client sc { env.config(), nclient, nthread, msg_len, nloop };
                sc.start_server_client();
                results[key(use_multi_thread, nsession, msg_len)] = sc.msg_num_per_sec();
            }
        }
    }
    show_result_summary(use_multi_thread_list, nsession_list, msg_len_list, results);
    //
    env.teardown();
}
