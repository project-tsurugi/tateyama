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
#include <numeric>

namespace tateyama::api::endpoint::ipc {

class one_mt_client_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string req_message { req->payload() };
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(req_message));
        return true;
    }
};

class ipc_one_mt_client_test_server_client: public server_client_base {
public:
    ipc_one_mt_client_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
            std::vector<std::size_t> &req_len_list, int nthread = 2, int nloop = 1000) :
            server_client_base(cfg), req_len_list_(req_len_list), nthread_(nthread), nloop_(nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<one_mt_client_service>();
    }

    void server() override {
        watch_dog wd { max_sec };
        //
        server_client_base::server();
        //
        std::size_t msg_num = nthread_ * nloop_ * 2 * req_len_list_.size();
        std::size_t len_sum = nthread_ * nloop_ * 2 * std::reduce(req_len_list_.cbegin(), req_len_list_.cend());
        std::cout << "nthread=" << nthread_;
        std::cout << ", nloop=" << nloop_;
        std::cout << ", max_len=" << req_len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    void client() override {
        watch_dog wd { max_sec };
        std::vector < std::thread > threads;
        threads.reserve(nthread_);
        for (int i = 0; i < nthread_; i++) {
            threads.emplace_back([this] {
                ipc_client client { cfg_ };
                for (int i = 0; i < nloop_; i++) {
                    for (std::size_t req_len : req_len_list_) {
                        std::string req_message;
                        make_dummy_message(client.session_id(), req_len, req_message);
                        client.send(one_mt_client_service::tag, req_message);
                        //
                        std::string res_message;
                        client.receive(res_message);
                        EXPECT_EQ(req_message, res_message);
                    }
                }
            });
        }
        for (std::thread &th : threads) {
            if (th.joinable()) {
                th.join();
            }
        }
    }

private:
    static constexpr int max_sec = 60;

    int nthread_ { };
    std::vector<std::size_t> req_len_list_;
    int nloop_ { };
};

class ipc_one_mt_client_test: public ipc_test_base {
};

TEST_F(ipc_one_mt_client_test, test_fixed_size_only) {
    std::vector<std::size_t> nthread_list {1, 4, 8, 32, 96};
    std::vector<std::size_t> maxlen_list {128, 256, 512};
    std::vector<std::size_t> req_len_list;
    for (std::size_t maxlen : maxlen_list) {
        req_len_list.clear();
        req_len_list.push_back(maxlen);
        int nloop = (maxlen <= 1024 ? 10000 : 1000);
        for (int nthread : nthread_list) {
            if (nthread > ipc_max_session_) {
                // NOTE: causes tateyama-server error
                continue;
            }
            ipc_one_mt_client_test_server_client sc {cfg_, req_len_list, nthread, nloop};
            sc.start_server_client();
        }
    }
}

} // namespace tateyama::api::endpoint::ipc
