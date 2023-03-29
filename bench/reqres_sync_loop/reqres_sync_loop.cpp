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

#include "resource_summary.h"

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
        maxsec_ = 300;
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<echo_service>();
    }

    static void result_header() {
        std::cout << "# session_num, multi_thread_or_procs, msg_len, nloop, elapse_sec, msg_num/sec, GB/sec"
                << std::endl;
    }

    void server() override {
        rusage_diff r_server{RUSAGE_SELF};
        rusage_diff r_clients{RUSAGE_CHILDREN};
        //
        server_client_base::server();
        //
        rusage_server_ = r_server.diff();
        rusage_clients_ = r_clients.diff();
        //
        std::cout << nworker_;
        std::cout << "," << (nthread_ > 0 ? "mt" : "mp"); // NOLINT
        std::cout << "," << msg_len_;
        std::cout << "," << nloop_;
        //
        std::size_t msg_num = nloop_ * 2 * nworker_;
        std::size_t len_sum = msg_num * msg_len_;
        real_sec_ = server_elapse_.sec();
        double gb_len = len_sum / (1024.0 * 1024.0 * 1024.0);
        msg_num_per_sec_ = msg_num / real_sec_;
        std::cout << "," << std::fixed << std::setprecision(6) << real_sec_;
        std::cout << "," << std::fixed << std::setprecision(1) << msg_num_per_sec_;
        std::cout << "," << std::fixed << std::setprecision(1) << gb_len / real_sec_;
        std::cout << std::endl;
    }

    [[nodiscard]] double real_sec() const {
        return real_sec_;
    }

    [[nodiscard]] double msg_num_per_sec() const {
        return msg_num_per_sec_;
    }

    [[nodiscard]] struct rusage& rusage_server() {
        return rusage_server_;
    }

    [[nodiscard]] struct rusage& rusage_clients() {
        return rusage_clients_;
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

    double real_sec_ { };
    double msg_num_per_sec_ { };
    struct rusage rusage_server_ { };
    struct rusage rusage_clients_ { };
};

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
    std::map<std::string, double> real_secs { };
    resource_summary res_summary { };
    reqres_sync_loop_server_client::result_header();
    for (bool use_multi_thread : use_multi_thread_list) {
        for (int nsession : nsession_list) {
            int nclient = (use_multi_thread ? 1 : nsession);
            int nthread = (use_multi_thread ? nsession : 0);
            for (std::size_t msg_len : msg_len_list) {
                reqres_sync_loop_server_client sc { env.config(), nclient, nthread, msg_len, nloop };
                sc.start_server_client();
                std::string k = key(use_multi_thread, nsession, msg_len);
                results[k] = sc.msg_num_per_sec();
                real_secs[k] = sc.real_sec();
                res_summary.add(k, sc.rusage_server(), sc.rusage_clients());
            }
        }
    }
    show_result_summary(use_multi_thread_list, nsession_list, msg_len_list, results, "throughput [msg/sec]");
    show_result_summary(use_multi_thread_list, nsession_list, msg_len_list, real_secs, "real_time [sec]", 6);
    res_summary.output(use_multi_thread_list, nsession_list, msg_len_list);
    //
    env.teardown();
}
