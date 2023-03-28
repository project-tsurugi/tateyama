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
#include "reqres_sync_loop/show_result.h"

using namespace tateyama::api::endpoint::ipc;

static void ASSERT_EQ(tateyama::status st1, tateyama::status st2) {
    if (st1 != st2) {
        std::cout << boost::stacktrace::stacktrace();
        std::exit(1);
    }
}

class data_channel_write_service: public server_service_base {
public:
    data_channel_write_service(std::size_t write_len, std::size_t write_nloop) :
            write_len_(write_len), write_nloop_(write_nloop) {
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        resultset_param param { payload };
        //
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        ASSERT_EQ(tateyama::status::ok, res->acquire_channel(param.name_, channel));
        std::shared_ptr<tateyama::api::server::writer> writer;
        ASSERT_EQ(tateyama::status::ok, channel->acquire(writer));
        //
        res->session_id(req->session_id());
        ASSERT_EQ(tateyama::status::ok, res->body(payload));
        //
        std::size_t write_len = param.write_lens_[0];
        std::string data;
        make_dummy_message(req->session_id(), write_len, data);
        for (std::size_t i = 0; i < param.write_nloop_; i++) {
            writer->write(data.c_str(), data.length());
            writer->commit();
        }
        ASSERT_EQ(tateyama::status::ok, channel->release(*writer));
        ASSERT_EQ(tateyama::status::ok, res->release_channel(*channel));
        return true;
    }

private:
    std::size_t write_len_ { };
    std::size_t write_nloop_ { };
};

class data_channel_write_server_client: public server_client_base {
public:
    data_channel_write_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nclient,
            int nthread, std::size_t write_len, std::size_t write_nloop) :
            server_client_base(cfg, nclient, nthread), write_len_(write_len), write_nloop_(write_nloop) {
        maxsec_ = 300;
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<data_channel_write_service>(write_len_, write_nloop_);
    }

    static void result_header() {
        std::cout << "# session_num, multi_thread_or_procs, write_len, write_nloop, elapse_sec, msg_num/sec, GB/sec"
                << std::endl;
    }

    void server() override {
        server_client_base::server();
        //
        std::cout << nworker_;
        std::cout << "," << (nthread_ > 0 ? "mt" : "mp"); // NOLINT
        std::cout << "," << write_len_;
        std::cout << "," << write_nloop_;
        //
        std::int64_t msec = server_elapse_.msec();
        std::size_t msg_num = write_nloop_ * nworker_;
        std::size_t len_sum = msg_num * write_len_;
        double sec = msec / 1000.0;
        double gb_len = len_sum / (1024.0 * 1024.0 * 1024.0);
        msg_num_per_sec_ = msg_num / sec;
        gb_per_sec_ = gb_len / sec;
        std::cout << "," << std::fixed << std::setprecision(3) << sec;
        std::cout << "," << std::fixed << std::setprecision(1) << msg_num_per_sec_;
        std::cout << "," << std::fixed << std::setprecision(2) << gb_per_sec_;
        std::cout << std::endl;
    }

    [[nodiscard]] double msg_num_per_sec() const {
        return msg_num_per_sec_;
    }

    [[nodiscard]] double gb_per_sec() const {
        return gb_per_sec_;
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string resultset_name { "resultset-" + client.session_name() };
        std::vector<std::size_t> len_list { write_len_ };
        resultset_param param { resultset_name, len_list, write_nloop_ };
        std::string req_message;
        param.to_string(req_message);
        std::string res_message;
        client.send(data_channel_write_service::tag, req_message);
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
    double msg_num_per_sec_ { };
    double gb_per_sec_ { };
};

int main(int argc, char **argv) {
    ipc_test_env env;
    env.setup();
    //
    std::vector<int> nsession_list { 1, 2, 4, 8 };
    for (int n = 16; n <= env.ipc_max_session(); n += 8) {
        nsession_list.push_back(n);
    }
    std::vector<std::size_t> msg_len_list { 0, 1, 2, 4, 8, 16, 32, 64 }; // [KB]
    std::vector<bool> use_multi_thread_list { true, false };
    const std::size_t nloop_default = 100'000;
    //
    std::map<std::string, double> msg_sec_results { };
    std::map<std::string, double> gb_sec_results { };
    for (bool use_multi_thread : use_multi_thread_list) {
        for (int nsession : nsession_list) {
            int nclient = (use_multi_thread ? 1 : nsession);
            int nthread = (use_multi_thread ? nsession : 0);
            for (std::size_t msg_len : msg_len_list) {
                std::size_t write_len;
                if (msg_len == 0) {
                    write_len = 0;
                } else {
                    write_len = 1024 * msg_len - tateyama::common::wire::length_header::size;
                }
                std::size_t nloop = (msg_len <= 16 ? 10 * nloop_default : nloop_default);
                data_channel_write_server_client sc { env.config(), nclient, nthread, write_len, nloop };
                sc.start_server_client();
                std::string k = key(use_multi_thread, nsession, msg_len);
                msg_sec_results[k] = sc.msg_num_per_sec();
                gb_sec_results[k] = sc.gb_per_sec();
            }
        }
    }
    show_result_summary(use_multi_thread_list, nsession_list, msg_len_list, msg_sec_results, "[msg/sec]");
    show_result_summary(use_multi_thread_list, nsession_list, msg_len_list, gb_sec_results, "[GB/sec]", 2);
    //
    env.teardown();
}
