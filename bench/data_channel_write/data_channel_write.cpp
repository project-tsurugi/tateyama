/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include "server_client_bench_base.h"
#include "bench_result_summary.h"
#include "main_args.h"

using namespace tateyama::endpoint::ipc;

class data_channel_write_service: public server_service_base {
public:
    data_channel_write_service(std::size_t write_len, std::size_t write_nloop) :
            write_len_(write_len), write_nloop_(write_nloop) {
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        //
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        ASSERT_OK(res->acquire_channel(payload, channel));
        std::shared_ptr<tateyama::api::server::writer> writer;
        ASSERT_OK(channel->acquire(writer));
        //
        res->session_id(req->session_id());
        ASSERT_OK(res->body(payload));
        //
        std::string data;
        make_dummy_message(req->session_id(), write_len_, data);
        for (std::size_t i = 0; i < write_nloop_; i++) {
            ASSERT_OK(writer->write(data.c_str(), data.length()));
            ASSERT_OK(writer->commit());
        }
        ASSERT_OK(channel->release(*writer));
        ASSERT_OK(res->release_channel(*channel));
        return true;
    }

private:
    std::size_t write_len_ { };
    std::size_t write_nloop_ { };
};

class data_channel_write_server_client: public server_client_bench_base {
public:
    data_channel_write_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc,
            int nthread, std::size_t write_len, std::size_t write_nloop) :
            server_client_bench_base(cfg, nproc, nthread), write_len_(write_len), write_nloop_(write_nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<data_channel_write_service>(write_len_, write_nloop_);
    }

    static void show_result_header() {
        std::cout << "# session_num, multi_thread_or_procs, write_len, write_nloop, elapse_sec, msg_num/sec, GB/sec"
                << std::endl;
    }

    void show_result_entry(double sec, double msg_num_per_sec, double gb_per_sec) {
        std::cout << nworker_;
        std::cout << "," << (nthread_ > 0 ? "mt" : "mp"); // NOLINT
        std::cout << "," << write_len_;
        std::cout << "," << write_nloop_;
        std::cout << "," << std::fixed << std::setprecision(6) << sec;
        std::cout << "," << msg_num_per_sec;
        std::cout << "," << std::fixed << std::setprecision(2) << gb_per_sec;
        std::cout << std::endl;
    }

    void server() override {
        server_client_bench_base::server();
        //
        std::size_t msg_num = write_nloop_ * nworker_;
        std::size_t len_sum = msg_num * write_len_;
        double sec = real_sec();
        double gb_len = len_sum / (1024.0 * 1024.0 * 1024.0);
        double msg_num_per_sec = msg_num / sec;
        double gb_per_sec = gb_len / sec;
        //
        add_result_value(info_type::nloop, write_nloop_);
        add_result_value(info_type::throughput_msg_per_sec, msg_num_per_sec);
        add_result_value(info_type::throughput_gb_per_sec, gb_per_sec);
        //
        show_result_entry(sec, msg_num_per_sec, gb_per_sec);
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string resultset_name { "resultset-" + client.session_name() };
        client.send(data_channel_write_service::tag, resultset_name);
        std::string res_message;
        client.receive(res_message);
        //
        resultset_wires_container *rwc = client.create_resultset_wires();
        rwc->connect(resultset_name);
        while (true) {
            std::string_view chunk = rwc->get_chunk();
            if (chunk.length() == 0 && rwc->is_eor()) {
                break;
            }
            rwc->dispose();
        }
        client.dispose_resultset_wires(rwc);
    }

private:
    std::size_t write_len_ { };
    std::size_t write_nloop_ { };
};

static inline std::size_t KB(int size) {
    return 1024UL * size;
}

static void bench_all() {
    ipc_test_env env;
    env.setup();
    //
    std::vector<int> nsession_list { 1, 2, 4, 8, 16 };
    for (int n = 24; n <= env.ipc_max_session(); n += 16) {
        nsession_list.push_back(n);
    }
    std::vector<std::size_t> msg_len_list { 0, 128, 256, 512, KB(1), KB(2), KB(4), KB(8), KB(16), KB(32), KB(64) };
    std::for_each(msg_len_list.begin(), msg_len_list.end(), [](std::size_t &len) {
        if (len > 0) {
            len -= tateyama::common::wire::length_header::size;
        }
    });
    std::vector<bool> use_multi_thread_list { true, false };
    const std::size_t nloop_default = 100'000;
    //
    data_channel_write_server_client::show_result_header();
    bench_result_summary result_summary { use_multi_thread_list, nsession_list, msg_len_list };
    for (bool use_multi_thread : use_multi_thread_list) {
        for (int nsession : nsession_list) {
            int nproc = (use_multi_thread ? 1 : nsession);
            int nthread = (use_multi_thread ? nsession : 0);
            for (std::size_t msg_len : msg_len_list) {
                std::size_t nloop { };
                if (msg_len == 0) {
                    nloop = 1000 * nloop_default;
                } else {
                    nloop = (msg_len <= KB(32) ? 10 * nloop_default : nloop_default);
                    if (nsession >= 16) {
                        nloop /= 2;
                    }
                }
                data_channel_write_server_client sc { env.config(), nproc, nthread, msg_len, nloop };
                sc.start_server_client();
                result_summary.add(use_multi_thread, nsession, msg_len, sc.result());
            }
        }
    }
    result_summary.show();
    //
    env.teardown();
}

static void bench_once(std::vector<std::string> &args) {
    ipc_test_env env;
    env.setup();
    //
    int nsession { std::stoi(args[1]) };
    bool use_multi_thread { args[2] == "mt" };
    std::size_t msg_len { std::stoull(args[3]) };
    std::size_t nloop { std::stoull(args[4]) };
    int nproc = (use_multi_thread ? 1 : nsession);
    int nthread = (use_multi_thread ? nsession : 0);
    //
    data_channel_write_server_client sc { env.config(), nproc, nthread, msg_len, nloop };
    sc.start_server_client();
    //
    env.teardown();
}

static void help(std::vector<std::string> &args) {
    std::cout << "Usage: " << args[0] << " [nsession {mt|mp} msg_len nloop]" << std::endl;
    std::cout << "\tex: " << args[0] << " 8 mt 512 100000" << std::endl;
    std::cout << "\tex: " << args[0] << " 16 mp 4192 10000" << std::endl;
    std::cout << "\tex: " << args[0] << std::endl;
}

int main(int argc, char **argv) {
    std::vector<std::string> args { };
    to_args(argc, argv, args);
    switch (argc) {
    case 1:
        bench_all();
        break;
    case 5:
        bench_once(args);
        break;
    default:
        help(args);
        break;
    }
}
