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
#include "ipc_client.h"
#include <numeric>

namespace tateyama::api::endpoint::ipc {

void make_channel_name(const std::string prefix, const int idx, std::string &name) {
    name = prefix;
    name += "-";
    name += std::to_string(idx);
}

class resultset_multi_channel_service: public server_service_base {
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
        std::string channel_name;
        make_channel_name(param.name_, param.ch_index_, channel_name);
        EXPECT_EQ(tateyama::status::ok, res->acquire_channel(channel_name, channel));
        EXPECT_NE(channel.get(), nullptr);
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body(payload));
        //
        msg_info info { req, param.ch_index_ };
        for (std::size_t i = 0; i < param.write_nloop_; i++) {
            info.set_i(i);
            for (std::size_t len : param.write_lens_) {
                std::string part;
                info.to_string(len, part);
                std::string data;
                make_dummy_message(part, len, data);
                EXPECT_EQ(len, get_message_len(data));
                writer->write(data.c_str(), data.length());
                writer->commit();
            }
        }
        EXPECT_EQ(tateyama::status::ok, channel->release(*writer));
        EXPECT_EQ(tateyama::status::ok, res->release_channel(*channel));
        return true;
    }
};

class ipc_resultset_multi_channel_test_server_client: public server_client_base {
public:
    ipc_resultset_multi_channel_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
            int nclient, int nthread, std::vector<std::size_t> &len_list, int nloop, std::size_t write_nloop,
            std::size_t nchannel) :
            server_client_base(cfg, nclient, nthread), len_list_(len_list), nloop_(nloop), write_nloop_(write_nloop), nchannel_(
                    nchannel) {
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<resultset_multi_channel_service>();
    }

    void server() override {
        server_client_base::server();
        //
        std::size_t msg_num = nworker_ * nloop_ * write_nloop_ * nchannel_ * len_list_.size();
        std::size_t len_sum = nworker_ * nloop_ * write_nloop_ * nchannel_
                * std::reduce(len_list_.cbegin(), len_list_.cend());
        std::cout << "nloop=" << nloop_;
        std::cout << ", write_nloop=" << write_nloop_;
        std::cout << ", nchannel=" << nchannel_;
        std::cout << ", max_len=" << len_list_.back() << ", ";
        server_client_base::server_dump(msg_num, len_sum);
    }

    void channel_test(ipc_client &client, resultset_wires_container *rwc, const std::string &channel_prefix,
            const std::size_t ch_index) {
        resultset_param param { channel_prefix, len_list_, write_nloop_, ch_index, 1 };
        std::string req_message;
        param.to_string(req_message);
        std::string res_message;
        client.send(resultset_multi_channel_service::tag, req_message);
        int receive_count = 0;
        bool receive_ok = false;
        do {
            try {
                client.receive(res_message);
                receive_ok = true;
            } catch (std::exception &e) {
                std::cout << e.what() << std::endl;
                receive_count++;
                if (receive_count > 10) {
                    FAIL();
                }
                std::cout << "retry receive response : " << receive_count << std::endl;
            }
        } while (!receive_ok);
        EXPECT_EQ(req_message, res_message);
        //
        std::string channel_name;
        make_channel_name(channel_prefix, ch_index, channel_name);
        rwc->connect(channel_name);
        std::size_t nread = 0;
        while (true) {
            std::string_view chunk = rwc->get_chunk();
            if (chunk.length() == 0 && rwc->is_eor()) {
                break;
            }
            std::size_t len = get_message_len(chunk);
            std::string data { chunk };
            if (chunk.length() < len) {
                std::string_view chunk2 = rwc->get_chunk();
                EXPECT_EQ(chunk2.length(), len - chunk.length());
                data += chunk2;
            }
            rwc->dispose();
            EXPECT_EQ(len, data.length());
            EXPECT_TRUE(check_dummy_message(data));
            nread++;
        }
        EXPECT_EQ(write_nloop_ * len_list_.size(), nread);
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string channel_prefix { "resultset-" + client.session_name() };
        //
        for (int i = 0; i < nloop_; i++) {
            resultset_wires_container *rwc = client.create_resultset_wires();
            for (std::size_t ch_index = 0; ch_index < nchannel_; ch_index++) {
                channel_test(client, rwc, channel_prefix, ch_index);
            }
            EXPECT_TRUE(rwc->is_eor());
            EXPECT_EQ(rwc->get_chunk().length(), 0);
            client.dispose_resultset_wires(rwc);
        }
    }

private:
    int nloop_ { };
    std::size_t write_nloop_ { };
    std::size_t nchannel_ { };
    std::vector<std::size_t> &len_list_;
};

class ipc_resultset_multi_channel_test: public ipc_test_base {
};

static const std::vector<std::size_t> nclient_list { 1, 2 }; // NOLINT
static const std::vector<std::size_t> nthread_list { 0, 1, 2 }; // NOLINT
static const std::vector<std::size_t> nchannel_list { 2, 4, 8 }; // NOLINT

TEST_F(ipc_resultset_multi_channel_test, fixed_size_only) {
    const std::size_t maxlen = ipc_client::resultset_record_maxlen;
    std::vector<std::size_t> len_list { 128, maxlen / 2 + 10 };
    for (int nclient : nclient_list) {
        for (int nthread : nthread_list) {
            for (std::size_t nchannel : nchannel_list) {
                for (std::size_t len : len_list) {
                    std::vector<std::size_t> list { len };
                    ipc_resultset_multi_channel_test_server_client sc { cfg_, nclient, nthread, list, 10, 10, nchannel };
                    sc.start_server_client();
                }
            }
        }
    }
}

} // namespace tateyama::api::endpoint::ipc
