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
#include "ipc_gtest_base.h"
#include <numeric>

namespace tateyama::api::endpoint::ipc {

class resultset_writer_limit_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        resultset_param param { payload };
        EXPECT_GT(param.name_.length(), 0);
        EXPECT_GT(param.write_nloop_, 0);
        EXPECT_GT(param.value_, 0);
        EXPECT_GT(param.write_lens_.size(), 0);
        const int nwriter = param.value_; // use as number of writers
        //
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(tateyama::status::ok, res->acquire_channel(param.name_, channel));
        EXPECT_NE(channel.get(), nullptr);
        std::vector<std::shared_ptr<tateyama::api::server::writer>> writer_list { };
        for (int i = 0; i < nwriter; i++) {
            std::shared_ptr<tateyama::api::server::writer> writer;
            EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
            writer_list.emplace_back(std::move(writer));
        }
        //
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(payload));
        //
        for (std::size_t i = 0; i < param.write_nloop_; i++) {
            msg_info info { req, i };
            for (std::size_t len : param.write_lens_) {
                for (int j = 0; j < nwriter; j++) {
                    info.set_index(j);
                    std::string part;
                    // part will be "len:session_id:i:j." such as "32768:1:0:1." etc.
                    info.to_string(len, part);
                    std::string data;
                    make_dummy_message(part, len, data);
                    //
                    std::shared_ptr<tateyama::api::server::writer> &writer = writer_list[j];
                    EXPECT_EQ(tateyama::status::ok, writer->write(data.c_str(), data.length()));
                    EXPECT_EQ(tateyama::status::ok, writer->commit());
                }
            }
        }
        for (auto writer : writer_list) {
            EXPECT_EQ(tateyama::status::ok, channel->release(*writer));
        }
        EXPECT_EQ(tateyama::status::ok, res->release_channel(*channel));
        return true;
    }
};

class ipc_resultset_writer_limit_test_server_client: public server_client_gtest_base {
public:
    ipc_resultset_writer_limit_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
            int nclient, int nthread, std::vector<std::size_t> &len_list, int nloop, std::size_t write_nloop,
            std::size_t nwriter) :
            server_client_gtest_base(cfg, nclient, nthread), len_list_(len_list), nloop_(nloop), write_nloop_(
                    write_nloop), nwriter_(nwriter) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<resultset_writer_limit_service>();
    }

    void server() override {
        server_client_base::server();
        //
        std::size_t msg_num = nworker_ * nloop_ * write_nloop_ * nwriter_ * len_list_.size();
        std::size_t len_sum = nworker_ * nloop_ * write_nloop_ * nwriter_
                * std::reduce(len_list_.cbegin(), len_list_.cend());
        std::cout << "nloop=" << nloop_;
        std::cout << ", nwriter=" << nwriter_;
        std::cout << ", write_nloop=" << write_nloop_;
        std::cout << ", max_len=" << len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    bool client_thread_body(ipc_client &client) {
        std::string resultset_name { "resultset-" + client.session_name() };
        resultset_param param { resultset_name, len_list_, write_nloop_, nwriter_ };
        std::string req_message;
        param.to_string(req_message);
        std::string res_message;
        //
        client.send(resultset_writer_limit_service::tag, req_message);
        client.receive(res_message);
        EXPECT_EQ(req_message, res_message);
        //
        std::set<std::string> received_parts { };
        /*
         * Receive all message until is_eor() is true
         */
        resultset_wires_container *rwc = client.create_resultset_wires();
        rwc->connect(resultset_name);
        while (true) {
            std::string_view chunk = rwc->get_chunk();
            if (chunk.length() == 0 && rwc->is_eor()) {
                break;
            }
            // the head of message always has it's length
            std::string data { chunk };
            std::size_t len = get_message_len(chunk);
            if (chunk.length() < len) {
                // The message was split because it was received beyond ring buffer boundary.
                // concatenate 2nd chunk to 1st.
                std::string_view chunk2 = rwc->get_chunk();
                EXPECT_EQ(chunk2.length(), len - chunk.length());
                data += chunk2;
            }
            rwc->dispose();
            EXPECT_EQ(len, data.length());
            EXPECT_TRUE(check_dummy_message(data));
            // save the session unique part of the message.
            std::string part { get_message_part(data) };
            received_parts.emplace(part);
        }
        EXPECT_TRUE(rwc->is_eor());
        EXPECT_EQ(rwc->get_chunk().length(), 0);
        client.dispose_resultset_wires(rwc);
        /*
         * Check all message was received
         */
        bool missing_data = false;
        EXPECT_EQ(received_parts.size(), write_nloop_ * nwriter_ * len_list_.size());
        for (std::size_t i = 0; i < write_nloop_; i++) {
            msg_info info { client.session_id(), i };
            for (std::size_t len : len_list_) {
                for (int j = 0; j < nwriter_; j++) {
                    info.set_index(j);
                    std::string part;
                    // part will be "len:session_id:i:j." such as "32768:1:0:1." etc.
                    info.to_string(len, part);
                    auto it = received_parts.find(part);
                    if (it != received_parts.end()) {
                        EXPECT_TRUE(true);
                    } else {
                        std::cout << "missing message : i=" << i << ", len=" << len << ", writer_idx=" << j
                                << ", nwriter=" << nwriter_ << std::endl;
                        missing_data = true;
                    }
                }
            }
        }
        return missing_data;
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        //
        for (int i = 0; i < nloop_; i++) {
            bool missing_data = client_thread_body(client);
            if (missing_data) {
                EXPECT_FALSE(missing_data);
                break;
            }
        }
    }

private:
    int nloop_ { };
    std::size_t write_nloop_ { };
    std::size_t nwriter_ { };
    std::vector<std::size_t> &len_list_;
};

// tateyama/src/tateyama/endpoint/ipc/bootstrap/server_wires_impl.h
// - private writer_count = 16;
static constexpr int max_writer_count = 16;
static const std::vector<std::size_t> nwriter_list { 1, 4, max_writer_count }; // NOLINT

class ipc_resultset_writer_limit_test: public ipc_gtest_base {
};

TEST_F(ipc_resultset_writer_limit_test, single_client) {
    const int nclient = 1;
    const int nthread = 0;
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { maxlen / 2 + 10 };
    const int nloop = 10;
    std::vector<std::size_t> write_nloop_list { 10, 100 };
    for (std::size_t write_nloop : write_nloop_list) {
        for (std::size_t len : len_list) {
            std::vector<std::size_t> list { len };
            for (std::size_t nwriter : nwriter_list) {
                ipc_resultset_writer_limit_test_server_client sc { cfg_, nclient, nthread, list, nloop, write_nloop,
                        nwriter };
                sc.start_server_client();
            }
        }
    }
}

TEST_F(ipc_resultset_writer_limit_test, multi_clients) {
    const std::vector<std::size_t> nclient_list { 2, 4 };
    const std::vector<std::size_t> nthread_list { 2, 4 };
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { maxlen / 2 + 10 };
    for (int nclient : nclient_list) {
        for (int nthread : nthread_list) {
            for (std::size_t len : len_list) {
                std::vector<std::size_t> list { len };
                for (std::size_t nwriter : nwriter_list) {
                    ipc_resultset_writer_limit_test_server_client sc { cfg_, nclient, nthread, list, 10, 10, nwriter };
                    sc.start_server_client();
                }
            }
        }
    }
}

// NOTE: This test may cause SIGBUS because of allocation failure of shared memory.
// If this test failed, other continuous tests also failed by SIGBUS.
// Successfully ran at Intel i7-13700 (16C/24T), 64GB, Ubuntu 20.04.6 LTS.
TEST_F(ipc_resultset_writer_limit_test, DISABLED_max_session_max_writer_max_datalen) {
    const int nclient = 1;
    const int nthread = ipc_max_session_;
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { maxlen };
    for (std::size_t nwriter : nwriter_list) {
        ipc_resultset_writer_limit_test_server_client sc { cfg_, nclient, nthread, len_list, 2, 10, nwriter };
        sc.start_server_client();
    }
}

// NOTE: (max+1)th channel->acquire(writer) never wake-up (wait until one writer is released). It's OK.
TEST_F(ipc_resultset_writer_limit_test, DISABLED_max_plus_1_writer) {
    const int nclient = 1;
    const int nthread = 0;
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { maxlen };
    ipc_resultset_writer_limit_test_server_client sc { cfg_, nclient, nthread, len_list, 1, 1, max_writer_count + 1 };
    sc.start_server_client();
}

} // namespace tateyama::api::endpoint::ipc
