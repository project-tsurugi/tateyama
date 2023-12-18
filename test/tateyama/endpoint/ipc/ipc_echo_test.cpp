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

namespace tateyama::endpoint::ipc {

class echo_service: public server_service_base {
public:
    explicit echo_service(std::vector<std::size_t> &len_list) :
            len_list_(len_list) {
        EXPECT_GT(len_list_.size(), 0);
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        size_t req_len = len_list_[index_ % len_list_.size()];
        index_++;
        // make another string object to reply
        std::string payload { req->payload() };
        EXPECT_TRUE(check_dummy_message(req->session_id(), payload));
        EXPECT_EQ(req_len, payload.length());
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(payload));
        return true;
    }

private:
    std::vector<std::size_t> &len_list_;
    int index_ = 0;
};

class ipc_echo_test_server_client: public server_client_gtest_base {
public:
    ipc_echo_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
            std::vector<std::size_t> &len_list, int nloop = 1000) :
            server_client_gtest_base(cfg), len_list_(len_list), nloop_(nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<echo_service>(len_list_);
    }

    void server() override {
        server_client_base::server();
        //
        std::size_t msg_num = nloop_ * 2 * len_list_.size();
        std::size_t len_sum = nloop_ * 2 * std::reduce(len_list_.cbegin(), len_list_.cend());
        std::cout << "nloop=" << nloop_;
        std::cout << ", max_len=" << len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        for (int i = 0; i < nloop_; i++) {
            for (std::size_t len : len_list_) {
                std::string req_message;
                make_dummy_message(client.session_id(), len, req_message);
                client.send(echo_service::tag, req_message);
                std::string res_message;
                client.receive(res_message);
                EXPECT_EQ(req_message, res_message);
            }
        }
    }

private:
    int nloop_ { };
    std::vector<std::size_t> &len_list_;
};

class ipc_echo_test: public ipc_gtest_base {
};

TEST_F(ipc_echo_test, test_fixed_size_only) {
    std::vector<std::size_t> maxlen_list { 32, 64, 128, 256, 512, 1024, 4 * 1024 };
    std::vector<std::size_t> len_list;
    for (std::size_t maxlen : maxlen_list) {
        len_list.clear();
        len_list.push_back(maxlen);
        int nloop = (maxlen <= 1024 ? 1000 : 300);
        ipc_echo_test_server_client sc { cfg_, len_list, nloop };
        sc.start_server_client();
    }
}

TEST_F(ipc_echo_test, test_power2_mixture) {
    // NOTE: server_wire_container_impl::request_buffer_size, response_buffer_size are private member.
    std::vector<std::size_t> maxlen_list { 128, 256, 1024, 4 * 1024, 32 * 1024 };
    std::vector<std::size_t> len_list;
    for (std::size_t maxlen : maxlen_list) {
        make_power2_length_list(len_list, maxlen + 1);
        int nloop = (maxlen <= 1024 ? 1000 : 300);
        ipc_echo_test_server_client sc { cfg_, len_list, nloop };
        sc.start_server_client();
    }
}

} // namespace tateyama::endpoint::ipc
