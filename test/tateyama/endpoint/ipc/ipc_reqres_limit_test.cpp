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
#include "ipc_gtest_base.h"
#include <numeric>

namespace tateyama::api::endpoint::ipc {

class reqres_limit_service: public server_service_base {
public:
    explicit reqres_limit_service(const std::vector<std::size_t> &req_len_list,
            const std::vector<std::size_t> &res_len_list) :
            req_len_list_(req_len_list), res_len_list_(res_len_list) {
        EXPECT_GT(req_len_list_.size(), 0);
        EXPECT_EQ(req_len_list_.size(), res_len_list_.size());
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        thread_local int index_ = 0;
        size_t req_len = req_len_list_[(index_ / res_len_list_.size()) % req_len_list_.size()];
        size_t res_len = res_len_list_[index_ % res_len_list_.size()];
        index_++;
        // make another string object to reply
        std::string req_message { req->payload() };
        EXPECT_EQ(req_len, req_message.length());
        EXPECT_TRUE(check_dummy_message(req_message));
        std::string part { get_message_part_without_len(req_message) };
        EXPECT_GT(part.length(), 0);
        part = std::to_string(res_len) + part;
        //
        std::string res_message;
        make_dummy_message(part, res_len, res_message);
        EXPECT_EQ(res_len, res_message.length());
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(res_message));
        return true;
    }
private:
    const std::vector<std::size_t> &req_len_list_;
    const std::vector<std::size_t> &res_len_list_;
};

class ipc_reqres_limit_test_server_client: public server_client_gtest_base {
public:
    ipc_reqres_limit_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
            const int nproc, const int nthread, const std::vector<std::size_t> &req_len_list,
            const std::vector<std::size_t> &res_len_list, int nloop) :
            server_client_gtest_base(cfg, nproc, nthread), req_len_list_(req_len_list), res_len_list_(res_len_list), nloop_(
                    nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<reqres_limit_service>(req_len_list_, res_len_list_);
    }

    void server() override {
        server_client_base::server();
        //
        std::size_t msg_num = nworker_ * nloop_ * req_len_list_.size() * res_len_list_.size();
        std::size_t len_sum = 0;
        std::size_t res_len_sum = std::reduce(res_len_list_.cbegin(), res_len_list_.cend());
        for (std::size_t req_len : req_len_list_) {
            len_sum += req_len + res_len_sum;
        }
        len_sum *= nworker_ * nloop_;
        std::cout << "nloop=" << nloop_;
        std::cout << ", req_max_len=" << req_len_list_.back();
        std::cout << ", res_max_len=" << res_len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::vector<std::size_t> msg_params { 0, 0, 0, 0, 0 };
        msg_params[0] = getpid();
        msg_params[1] = gettid();
        for (int i = 0; i < nloop_; i++) {
            msg_params[2] = i;
            for (std::size_t req_len : req_len_list_) {
                msg_params[3] = req_len;
                for (std::size_t res_len : res_len_list_) {
                    msg_params[4] = res_len;
                    std::string part;
                    params_to_string(req_len, msg_params, part);
                    std::string req_message;
                    make_dummy_message(part, req_len, req_message);
                    //
                    client.send(reqres_limit_service::tag, req_message);
                    std::string res_message;
                    client.receive(res_message);
                    //
                    EXPECT_EQ(res_message.length(), res_len);
                    EXPECT_TRUE(check_dummy_message(res_message));
                }
            }
        }
    }

private:
    int nloop_ { };
    const std::vector<std::size_t> &req_len_list_;
    const std::vector<std::size_t> &res_len_list_;
};

class ipc_reqres_limit_test: public ipc_gtest_base {
public:
    void make_list(std::size_t max_len, std::size_t range, std::vector<std::size_t> &list) {
        for (std::size_t len = max_len - range; len <= max_len + range; len++) {
            list.push_back(len);
        }
    }
};

// NOTE: server_wire_container_impl::request_buffer_size, response_buffer_size are private member.
static const std::size_t max_req_len = 32 * 1024;
static const std::size_t max_res_len = 64 * 1024;

static const std::vector<int> nproc_list { 1, 2 }; // NOLINT
static const std::vector<int> nthread_list { 0, 2 }; // NOLINT

TEST_F(ipc_reqres_limit_test, req_res_maxlen_half) {
    std::size_t range = sizeof(std::size_t) + 1;
    std::vector<std::size_t> req_len_list { };
    std::vector<std::size_t> res_len_list { };
    make_list(max_req_len / 2, range, req_len_list);
    make_list(max_res_len / 2, range, res_len_list);
    dump_length_list(req_len_list);
    dump_length_list(res_len_list);
    const int nloop = 2;
    for (int nproc : nproc_list) {
        for (int nthread : nthread_list) {
            ipc_reqres_limit_test_server_client sc { cfg_, nproc, nthread, req_len_list, res_len_list, nloop };
            sc.start_server_client();
        }
    }
}

TEST_F(ipc_reqres_limit_test, req_res_maxlen) {
    std::size_t range = sizeof(std::size_t) + 1;
    std::vector<std::size_t> req_len_list { };
    std::vector<std::size_t> res_len_list { };
    make_list(max_req_len, range, req_len_list);
    make_list(max_res_len, range, res_len_list);
    dump_length_list(req_len_list);
    dump_length_list(res_len_list);
    const int nloop = 2;
    for (int nproc : nproc_list) {
        for (int nthread : nthread_list) {
            ipc_reqres_limit_test_server_client sc { cfg_, nproc, nthread, req_len_list, res_len_list, nloop };
            sc.start_server_client();
        }
    }
}

TEST_F(ipc_reqres_limit_test, req_res_more_than_maxlen) {
    const int nproc = 2;
    const int nthread = 2;
    const std::size_t req_len_limit = 1024 * 1024;
    std::size_t req_len = max_req_len;
    std::size_t res_len = max_res_len;
    const int nloop = 2;
    while (req_len <= req_len_limit) {
        std::vector<std::size_t> req_len_list { req_len };
        std::vector<std::size_t> res_len_list { res_len };
        ipc_reqres_limit_test_server_client sc { cfg_, nproc, nthread, req_len_list, res_len_list, nloop };
        sc.start_server_client();
        req_len *= 2;
        res_len *= 2;
    }
}

} // namespace tateyama::api::endpoint::ipc
