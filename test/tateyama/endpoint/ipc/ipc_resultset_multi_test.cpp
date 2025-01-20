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

class resultset_multi_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        resultset_param param { payload };
        EXPECT_GT(param.name_.length(), 0);
        EXPECT_GT(param.write_nloop_, 0);
        EXPECT_GT(param.write_lens_.size(), 0);
        //
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(tateyama::status::ok, res->acquire_channel(param.name_, channel, default_writer_count));
        EXPECT_NE(channel.get(), nullptr);
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
        //
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body_head(payload));
        //
        for (std::size_t i = 0; i < param.write_nloop_; i++) {
            for (std::size_t len : param.write_lens_) {
                std::string data;
                make_dummy_message(req->session_id() + len + i, len, data);
                EXPECT_EQ(tateyama::status::ok, writer->write(data.c_str(), data.length()));
                EXPECT_EQ(tateyama::status::ok, writer->commit());
            }
        }
        EXPECT_EQ(tateyama::status::ok, channel->release(*writer));
        EXPECT_EQ(tateyama::status::ok, res->release_channel(*channel));
        return true;
    }
};

class ipc_resultset_multi_test_server_client: public server_client_gtest_base {
public:
    ipc_resultset_multi_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc,
            int nthread, std::vector<std::size_t> &len_list, int nloop, std::size_t write_nloop) :
            server_client_gtest_base(cfg, nproc, nthread), len_list_(len_list), nloop_(nloop), write_nloop_(
                    write_nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<resultset_multi_service>();
    }

    void server() override {
        server_client_base::server();
        //
        std::size_t msg_num = nworker_ * nloop_ * write_nloop_ * len_list_.size();
        std::size_t len_sum = nworker_ * nloop_ * write_nloop_ * std::reduce(len_list_.cbegin(), len_list_.cend());
        std::cout << "nloop=" << nloop_;
        std::cout << ", write_nloop=" << write_nloop_;
        std::cout << ", max_len=" << len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string resultset_name { "resultset-" + client.session_name() };
        resultset_param param { resultset_name, len_list_, write_nloop_ };
        std::string req_message;
        param.to_string(req_message);
        std::string res_message;
        //
        for (int i = 0; i < nloop_; i++) {
            client.send(resultset_multi_service::tag, req_message);
            client.receive(res_message);
            EXPECT_EQ(req_message, res_message);
            //
            resultset_wires_container *rwc = client.create_resultset_wires();
            rwc->connect(resultset_name);
            for (std::size_t j = 0; j < write_nloop_; j++) {
                for (std::size_t len : len_list_) {
                    std::string data { };
                    data.reserve(len);
                    // NOTE: One call of writer->write(data, len) (>= 16KB or so) may be gotten
                    // by more than one get_chunk(). You should concatenate every chunk data.
                    while (data.length() < len) {
                        std::string_view chunk = rwc->get_chunk();
                        if (chunk.length() == 0) {
                            break;
                        }
                        data += chunk;
                        rwc->dispose();
                    }
                    EXPECT_EQ(len, data.length());
                    EXPECT_TRUE(check_dummy_message(client.session_id() + len + j, data));
                }
            }
            // just wait until is_eor() is true
            while (!rwc->is_eor()) {
                EXPECT_EQ(rwc->get_chunk().length(), 0);
            }
            EXPECT_TRUE(rwc->is_eor());
            EXPECT_EQ(rwc->get_chunk().length(), 0);
            client.dispose_resultset_wires(rwc);
        }
    }

private:
    int nloop_ { };
    std::size_t write_nloop_ { };
    std::vector<std::size_t> &len_list_;
};

class ipc_resultset_multi_test: public ipc_gtest_base {
};

static const std::vector<std::size_t> nproc_list { 1, 2 }; // NOLINT
static const std::vector<std::size_t> nthread_list { 0, 1, 2 }; // NOLINT

TEST_F(ipc_resultset_multi_test, fixed_size_only) {
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { 1, 128, 1024 };
    for (int nproc : nproc_list) {
        for (int nthread : nthread_list) {
            for (std::size_t len : len_list) {
                std::vector<std::size_t> list { len };
                ipc_resultset_multi_test_server_client sc { cfg_, nproc, nthread, list, 10, 10 };
                sc.start_server_client();
            }
        }
    }
}

TEST_F(ipc_resultset_multi_test, DISABLED_fixed_size_only_maxlen) {
    const std::vector<std::size_t> nproc_big_list { 1, 2, 4 }; // NOLINT
    const std::vector<std::size_t> nthread_big_list { 1, 2, 4 }; // NOLINT
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { 2 * maxlen / 3, maxlen };
    for (int nproc : nproc_big_list) {
        for (int nthread : nthread_big_list) {
            for (std::size_t len : len_list) {
                std::vector<std::size_t> list { len };
                ipc_resultset_multi_test_server_client sc { cfg_, nproc, nthread, list, 10, 1000 };
                sc.start_server_client();
            }
        }
    }
}

TEST_F(ipc_resultset_multi_test, power2_mixture) {
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { };
    make_power2_length_list(len_list, maxlen);
    len_list.push_back(maxlen);
    dump_length_list(len_list);
    for (int nproc : nproc_list) {
        for (int nthread : nthread_list) {
            ipc_resultset_multi_test_server_client sc { cfg_, nproc, nthread, len_list, 10, 100 };
            sc.start_server_client();
        }
    }
}

TEST_F(ipc_resultset_multi_test, around_record_max) {
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    const std::size_t range = 10;
    std::vector<std::size_t> len_list { };
    // NOTE: len should be <= ipc_client::resultset_record_maxlen.
    // get_chunk() never wakeup if len > resultset_record_maxlen.
    // It's limitation of current implementation.
    for (int nproc : nproc_list) {
        for (int nthread : nthread_list) {
            for (std::size_t len = maxlen - range; len <= maxlen; len++) {
                std::vector<std::size_t> list { len };
                ipc_resultset_multi_test_server_client sc { cfg_, nproc, nthread, list, 10, 10 };
                sc.start_server_client();
            }
        }
    }
}

TEST_F(ipc_resultset_multi_test, DISABLED_many_send) {
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { 1, 128, maxlen };
    for (std::size_t len : len_list) {
        std::vector<std::size_t> list { len };
        for (int nproc : nproc_list) {
            for (int nthread : nthread_list) {
                ipc_resultset_multi_test_server_client sc { cfg_, nproc, nthread, list, 2, 10000 };
                sc.start_server_client();
            }
        }
    }
}

} // namespace tateyama::endpoint::ipc
