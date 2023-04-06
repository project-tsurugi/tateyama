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

#include "server_client_bench_base.h"
#include "bench_result_summary.h"

using namespace tateyama::api::endpoint::ipc;

class echo_service: public server_service_base {
public:
    echo_service(comm_type c_type) :
            comm_type_(c_type) {
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        switch (comm_type_) {
        case comm_type::sync:
        case comm_type::async:
            res->session_id(req->session_id());
            ASSERT_OK(res->body(req->payload()));
            break;
        case comm_type::async_both: {
            // NOTE: NOT WORK CORRECTLY. just made for test
            std::shared_ptr<tateyama::api::server::response> res2 = std::move(res);
            auto payload = req->payload();
            res2->session_id(req->session_id());
            std::thread([res2, payload] {
                ASSERT_OK(res2->body(payload));
            });
        }
            break;
        case comm_type::nores:
            break;
        }
        return true;
    }
private:
    comm_type comm_type_ { };
};

class reqres_sync_loop_server_client: public server_client_bench_base {
public:
    reqres_sync_loop_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc,
            int nthread, std::size_t msg_len, int nloop, comm_type c_type) :
            server_client_bench_base(cfg, nproc, nthread), msg_len_(msg_len), nloop_(nloop), comm_type_(c_type) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<echo_service>(comm_type_);
    }

    static void show_result_header() {
        std::cout << "# session_num, multi_thread_or_procs, comm_type, ";
        std::cout << "msg_len, nloop, elapse_sec, msg_num/sec, GB/sec" << std::endl;
    }

    void show_result_entry(double sec, double msg_num_per_sec, double gb_per_sec) {
        std::cout << nworker_;
        std::cout << "," << (nthread_ > 0 ? "mt" : "mp"); // NOLINT
        switch (comm_type_) {
        case comm_type::sync:
            std::cout << ",sync";
            break;
        case comm_type::async:
            std::cout << ",async";
            break;
        case comm_type::async_both:
            std::cout << ",async_both";
            break;
        case comm_type::nores:
            std::cout << ",nores";
            break;
        }
        std::cout << "," << msg_len_;
        std::cout << "," << nloop_;
        std::cout << "," << std::fixed << std::setprecision(6) << sec;
        std::cout << "," << msg_num_per_sec;
        std::cout << "," << std::fixed << std::setprecision(2) << gb_per_sec;
        std::cout << std::endl;
    }

    void server() override {
        server_client_bench_base::server();
        //
        std::size_t msg_num = nloop_ * nworker_;
        if (comm_type_ != comm_type::nores) {
            msg_num *= 2;
        }
        std::size_t len_sum = msg_num * msg_len_;
        double sec = real_sec();
        double gb_len = len_sum / (1024.0 * 1024.0 * 1024.0);
        double msg_num_per_sec = msg_num / sec;
        double gb_per_sec = gb_len / sec;
        //
        add_result_value(info_type::nloop, nloop_);
        add_result_value(info_type::throughput_msg_per_sec, msg_num_per_sec);
        add_result_value(info_type::throughput_gb_per_sec, gb_per_sec);
        //
        show_result_entry(sec, msg_num_per_sec, gb_per_sec);
    }

    void client_loop_sync(std::string &req_message, ipc_client &client) {
        std::string res_message;
        for (int i = 0; i < nloop_; i++) {
            client.send(echo_service::tag, req_message);
            client.receive(res_message);
        }
    }

    void client_loop_async(std::string &req_message, ipc_client &client) {
        std::thread th = std::thread([this, &client] {
            std::string res_message;
            for (int i = 0; i < nloop_; i++) {
                client.receive(res_message);
            }
        });
        for (int i = 0; i < nloop_; i++) {
            client.send(echo_service::tag, req_message);
        }
        if (th.joinable()) {
            th.join();
        }
    }

    void client_loop_nores(std::string &req_message, ipc_client &client) {
        for (int i = 0; i < nloop_; i++) {
            client.send(echo_service::tag, req_message);
        }
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string req_message;
        make_dummy_message(gettid(), msg_len_, req_message);
        switch (comm_type_) {
        case comm_type::sync:
            client_loop_sync(req_message, client);
            break;
        case comm_type::async:
        case comm_type::async_both:
            client_loop_async(req_message, client);
            break;
        case comm_type::nores:
            client_loop_nores(req_message, client);
            break;
        }
    }

private:
    std::size_t msg_len_;
    int nloop_ { };
    comm_type comm_type_ { };
};

static comm_type get_comm_type(int argc, char **argv) {
    if (argc <= 1) {
        return comm_type::sync;
    }
    char *param = argv[argc - 1];
    if (strcmp(param, "sync") == 0) {
        return comm_type::sync;
    }
    if (strcmp(param, "async") == 0) {
        return comm_type::async;
    }
    if (strcmp(param, "async_both") == 0) {
        return comm_type::async_both;
    }
    if (strcmp(param, "nores") == 0) {
        return comm_type::nores;
    }
    if ('0' <= param[0] && param[0] <= '9') {
        // without comm_type option.
        return comm_type::sync;
    }
    std::cout << "ERROR: invalid comm_type: " << param << std::endl;
    std::exit(1);
    return comm_type::sync; // not reach
}

static void bench_all(int argc, char **argv) {
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
    comm_type c_type = get_comm_type(argc, argv);
    //
    reqres_sync_loop_server_client::show_result_header();
    bench_result_summary result_summary { use_multi_thread_list, nsession_list, msg_len_list, c_type };
    for (bool use_multi_thread : use_multi_thread_list) {
        for (int nsession : nsession_list) {
            int nproc = (use_multi_thread ? 1 : nsession);
            int nthread = (use_multi_thread ? nsession : 0);
            for (std::size_t msg_len : msg_len_list) {
                reqres_sync_loop_server_client sc { env.config(), nproc, nthread, msg_len, nloop, c_type };
                sc.start_server_client();
                result_summary.add(use_multi_thread, nsession, msg_len, sc.result());
            }
        }
    }
    result_summary.show();
    //
    env.teardown();
}

static void bench_once(int argc, char **argv) {
    ipc_test_env env;
    env.setup();
    //
    bool use_multi_thread { strcmp(argv[1], "mt") == 0 };
    int nsession { std::atoi(argv[2]) };
    std::size_t msg_len { std::strtoull(argv[3], nullptr, 10) };
    int nloop { std::atoi(argv[4]) };
    comm_type c_type = get_comm_type(argc, argv);
    int nproc = (use_multi_thread ? 1 : nsession);
    int nthread = (use_multi_thread ? nsession : 0);
    //
    reqres_sync_loop_server_client::show_result_header();
    reqres_sync_loop_server_client sc { env.config(), nproc, nthread, msg_len, nloop, c_type };
    sc.start_server_client();
    //
    env.teardown();
}

static void help(char **argv) {
    std::cout << "Usage: " << argv[0] << " [{mt|mp} nsession msg_len nloop] [sync|async|async_both|nores]" << std::endl;
    std::cout << "\tex: " << argv[0] << std::endl;
    std::cout << "\tex: " << argv[0] << " async" << std::endl;
    std::cout << "\tex: " << argv[0] << " nores" << std::endl;
    std::cout << "\tex: " << argv[0] << " mt 8 512 100000" << std::endl;
    std::cout << "\tex: " << argv[0] << " mp 16 4192 10000" << std::endl;
    std::cout << "\tex: " << argv[0] << " mp 16 4192 10000 nores" << std::endl;
}

int main(int argc, char **argv) {
    switch (argc) {
    case 1:
        bench_all(argc, argv);
        break;
    case 2:
        if (strcmp(argv[1], "help") != 0) {
            bench_all(argc, argv);
        } else {
            help(argv);
        }
        break;
    case 5:
    case 6:
        bench_once(argc, argv);
        break;
    default:
        help(argv);
        break;
    }
}
