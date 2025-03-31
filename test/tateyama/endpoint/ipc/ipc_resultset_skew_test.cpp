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
#include <thread>
#include <chrono>

namespace tateyama::endpoint::ipc {

class resultset_skew_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        resultset_param param { payload };
        EXPECT_GT(param.name_.length(), 0);
        EXPECT_GT(param.write_nloop_, 0);
        EXPECT_EQ(param.write_lens_.size(), 1);
        auto len = param.write_lens_.at(0);
        //
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(tateyama::status::ok, res->acquire_channel(param.name_, channel, default_writer_count));
        EXPECT_NE(channel.get(), nullptr);
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
        std::shared_ptr<tateyama::api::server::writer> writer_short;
        EXPECT_EQ(tateyama::status::ok, channel->acquire(writer_short));
        //
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body_head(payload));
        //
        for (std::size_t i = 0; i < param.write_nloop_; i++) {
            std::string data;
            make_dummy_message(req->session_id() + len + i, len, data);
            EXPECT_EQ(tateyama::status::ok, writer->write(data.c_str(), data.length()));
            EXPECT_EQ(tateyama::status::ok, writer->commit());
        }
        EXPECT_EQ(tateyama::status::ok, channel->release(*writer));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::string data_short;
        make_dummy_message(req->session_id() + len + param.write_nloop_, len, data_short);
        EXPECT_EQ(tateyama::status::ok, writer_short->write(data_short.c_str(), data_short.length()));
        EXPECT_EQ(tateyama::status::ok, writer_short->commit());

        EXPECT_EQ(tateyama::status::ok, channel->release(*writer_short));
        EXPECT_EQ(tateyama::status::ok, res->release_channel(*channel));
        return true;
    }
};

class ipc_resultset_skew_test_server_client: public server_client_gtest_base {
public:
    ipc_resultset_skew_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc,
            int nthread, std::vector<std::size_t> &len_list, int nloop, std::size_t write_nloop) :
            server_client_gtest_base(cfg, nproc, nthread), len_list_(len_list), nloop_(nloop), write_nloop_(
                    write_nloop) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<resultset_skew_service>();
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
            client.send(resultset_skew_service::tag, req_message);
            client.receive(res_message);
            EXPECT_EQ(req_message, res_message);
            auto len = len_list_.at(0);
            //
            resultset_wires_container *rwc = client.create_resultset_wires();
            rwc->connect(resultset_name);
            for (std::size_t j = 0; j < write_nloop_; j++) {
                std::string data { };
                data.reserve(len);
                // NOTE: One call of writer->write(data, len) (>= 16KB or so) may be gotten
                // by more than one get_chunk(). You should concatenate every chunk data.
                while (data.length() < len) {
                    std::string_view chunk = rwc->get_chunk(0);
                    if (chunk.length() == 0) {
                        break;
                    }
                    data += chunk;
                    rwc->dispose();
                }
                EXPECT_EQ(len, data.length());
                EXPECT_TRUE(check_dummy_message(client.session_id() + len + j, data));
            }

            std::string data_short { };
            data_short.reserve(len);
            while (data_short.length() < len) {
                std::string_view chunk = rwc->get_chunk(0);
                if (chunk.length() == 0) {
                    break;
                }
                data_short += chunk;
                rwc->dispose();
            }
            EXPECT_EQ(len, data_short.length());
            EXPECT_TRUE(check_dummy_message(client.session_id() + len + write_nloop_, data_short));

            // just wait until is_eor() is true
            while (!rwc->is_eor()) {
                EXPECT_EQ(rwc->get_chunk(0).length(), 0);
            }
            EXPECT_TRUE(rwc->is_eor());
            EXPECT_EQ(rwc->get_chunk(0).length(), 0);
            client.dispose_resultset_wires(rwc);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

private:
    int nloop_ { };
    std::size_t write_nloop_ { };
    std::vector<std::size_t> &len_list_;
};

class ipc_resultset_skew_test: public ipc_gtest_base {
};

static const std::vector<std::size_t> nproc_list { 1 }; // NOLINT
static const std::vector<std::size_t> nthread_list { 0 }; // NOLINT

TEST_F(ipc_resultset_skew_test, fixed_size_only) {
    std::vector<std::size_t> len { 32768 };
    ipc_resultset_skew_test_server_client sc { cfg_, 1, 0, len, 1, 7 };
    sc.start_server_client();
}

} // namespace tateyama::endpoint::ipc
