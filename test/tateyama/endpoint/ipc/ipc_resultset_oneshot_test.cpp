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

namespace tateyama::api::endpoint::ipc {

static const std::string resultset_name { "resultset_1" }; // NOLINT

class resultset_oneshot_service: public server_service_base {
public:
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::string payload { req->payload() };
        std::size_t datalen = std::stoul(payload);
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(tateyama::status::ok, res->acquire_channel(resultset_name, channel));
        EXPECT_NE(channel.get(), nullptr);
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
        //
        res->session_id(req->session_id());
        EXPECT_EQ(tateyama::status::ok, res->body("writer ready!"));
        //
        std::string data;
        make_dummy_message(req->session_id(), datalen, data);
        EXPECT_EQ(datalen, data.length());
        std::cout << "server : call write() : " << datalen << std::endl;
        writer->write(data.c_str(), data.length());
        std::cout << "server : call commit()" << std::endl;
        writer->commit();
        std::cout << "server : commit done()" << std::endl;
        //
        EXPECT_EQ(tateyama::status::ok, channel->release(*writer));
        std::cout << "server : release() done" << std::endl;
        EXPECT_EQ(tateyama::status::ok, res->release_channel(*channel));
        std::cout << "server : release_channel() done" << std::endl;
        return true;
    }
};

class ipc_resultset_oneshot_test_server_client: public server_client_base {
public:
    ipc_resultset_oneshot_test_server_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
            std::size_t datalen) :
            server_client_base(cfg), datalen_(datalen) {
        maxsec = 3;
    }

    std::shared_ptr<tateyama::framework::service> create_server_service() override {
        return std::make_shared<resultset_oneshot_service>();
    }

    void server() override {
        server_client_base::server();
    }

    void client_thread() override {
        ipc_client client { cfg_ };
        std::string req_message { std::to_string(datalen_) };
        std::string res_message;
        client.send(resultset_oneshot_service::tag, req_message);
        client.receive(res_message);
        //
        resultset_wires_container *rwc = client.create_resultset_wires();
        rwc->connect(resultset_name);
        std::string data { };
        data.reserve(datalen_);
        // NOTE: One call of writer->write(data, len) (>= 16KB or so) may be gotten
        // by more than one get_chunk(). You should concatenate every chunk data.
        while (data.length() < datalen_) {
            std::cout << "client : call get_chunk()" << std::endl;
            std::string_view chunk = rwc->get_chunk();
            std::cout << "client : get_chunk() done : " << chunk.length() << std::endl;
            if (chunk.length() == 0) {
                break;
            }
            data += chunk;
            rwc->dispose();
            std::cout << "client : dispose() done" << std::endl;
        }
        // just wait until is_eor() is true
        while (!rwc->is_eor()) {
            EXPECT_EQ(rwc->get_chunk().length(), 0);
        }
        EXPECT_TRUE(rwc->is_eor());
        EXPECT_EQ(rwc->get_chunk().length(), 0);
        EXPECT_EQ(datalen_, data.length());
        EXPECT_TRUE(check_dummy_message(client.session_id(), data));
        client.dispose_resultset_wires(rwc);
    }

private:
    std::size_t datalen_;
};

class ipc_resultset_oneshot_test: public ipc_test_base {
};

TEST_F(ipc_resultset_oneshot_test, test_one) {
    ipc_resultset_oneshot_test_server_client sc { cfg_, 1 };
    sc.start_server_client();
}

TEST_F(ipc_resultset_oneshot_test, test_record_max) {
    ipc_resultset_oneshot_test_server_client sc { cfg_, ipc_client::resultset_record_maxlen };
    sc.start_server_client();
}

#ifdef NOT_PASS
// get_chunk() never wakeup. It's limitation of current implementation.
TEST_F(ipc_resultset_oneshot_test, test_record_max_plus_1) {
    ipc_resultset_oneshot_test_server_client sc {cfg_, ipc_client::resultset_record_maxlen + 1};
    sc.start_server_client();
}
#endif

} // namespace tateyama::api::endpoint::ipc
