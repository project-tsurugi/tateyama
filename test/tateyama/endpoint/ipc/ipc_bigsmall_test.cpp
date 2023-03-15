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

class bigsmall_service: public server_service_base {
public:
    bigsmall_service(std::vector<std::size_t> &req_len_list, std::vector<std::size_t> &res_len_list) :
            req_len_list_(req_len_list), res_len_list_(res_len_list) {
        EXPECT_GT(req_len_list_.size(), 0);
        EXPECT_GT(res_len_list_.size(), 0);
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        size_t req_len = req_len_list_[(index_ / res_len_list_.size()) % req_len_list_.size()];
        size_t res_len = res_len_list_[index_ % res_len_list_.size()];
        index_++;
        //
        std::string req_message { req->payload() };
        EXPECT_EQ(req_len, req_message.length());
        EXPECT_TRUE(check_dummy_message(req->session_id(), req_message));
        //
        std::string res_message;
        make_dummy_message(req->session_id(), res_len, res_message);
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(res_message));
        return true;
    }

private:
    std::vector<std::size_t> &req_len_list_;
    std::vector<std::size_t> &res_len_list_;
    int index_ = 0;
};

class ipc_bigsmall_test_server_client: public server_client_base {
public:
    ipc_bigsmall_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nloop,
            std::size_t req_max_length, std::size_t res_max_length) :
            server_client_base(cfg), nloop_(nloop) {
        make_power2_length_list(req_len_list_, req_max_length);
        EXPECT_GT(req_len_list_.size(), 0);
        make_power2_length_list(res_len_list_, res_max_length);
        EXPECT_GT(res_len_list_.size(), 0);
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared < bigsmall_service > (req_len_list_, res_len_list_);
    }

    void server() override {
        server_client_base::server();
        //
        std::size_t msg_num = nloop_ * req_len_list_.size() * res_len_list_.size();
        std::size_t len_sum = 0;
        std::size_t res_len_sum = std::reduce(res_len_list_.cbegin(), res_len_list_.cend());
        for (std::size_t req_len : req_len_list_) {
            len_sum += req_len + res_len_sum;
        }
        len_sum *= nloop_;
        std::cout << "nloop=" << nloop_;
        std::cout << ", req_max_len=" << req_len_list_.back();
        std::cout << ", res_max_len=" << res_len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        for (int i = 0; i < nloop_; i++) {
            for (std::size_t req_len : req_len_list_) {
                std::string req_message;
                make_dummy_message(client.session_id(), req_len, req_message);
                for (std::size_t res_len : res_len_list_) {
                    client.send(bigsmall_service::tag, req_message);
                    std::string res_message;
                    client.receive(res_message);
                    EXPECT_EQ(res_message.length(), res_len);
                    EXPECT_TRUE(check_dummy_message(client.session_id(), res_message));
                }
            }
        }
    }

private:
    int nloop_ { };
    std::vector<std::size_t> req_len_list_, res_len_list_;
};

class ipc_bigsmall_test: public ipc_test_base {
};

TEST_F(ipc_bigsmall_test, test1) {
    // NOTE: server_wire_container_impl::request_buffer_size, response_buffer_size are private member.
    std::vector<std::size_t> maxlen_list {32, 64, 128, 256, 1024};
    for (std::size_t maxlen : maxlen_list) {
        ipc_bigsmall_test_server_client sc {cfg_, 100, maxlen, maxlen};
        sc.start_server_client();
    }
}

TEST_F(ipc_bigsmall_test, test2) {
    // NOTE: server_wire_container_impl::request_buffer_size, response_buffer_size are private member.
    std::size_t maxlen = 4 * 1024;
    ipc_bigsmall_test_server_client sc {cfg_, 100, maxlen + 1, maxlen + 1};
    sc.start_server_client();
}

} // namespace tateyama::api::endpoint::ipc
