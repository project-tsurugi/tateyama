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
#include <iostream>

#include "tateyama/endpoint/stream/bootstrap/stream_worker.h"
#include "tateyama/endpoint/header_utils.h"

#include "stream_client.h"

#include <gtest/gtest.h>

static constexpr std::size_t my_session_id_ = 123;
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::string_view response_test_message = "opqrstuvwxyz";

namespace tateyama::server {
class info_service_for_test : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }
    std::string_view label() const noexcept { return __func__; }

    id_type id() const noexcept { return 100;  } // dummy
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        req_ = req;
        res->body(response_test_message);
        return true;
    }

    tateyama::api::server::request* request() {
        return req_.get();
    }

private:
    std::shared_ptr<tateyama::api::server::request> req_{};
};

class stream_listener_for_test {
public:
    stream_listener_for_test(info_service_for_test& service) : service_(service) {
    }
    void operator()() {
        while (true) {
            std::shared_ptr<tateyama::common::stream::stream_socket> stream{};
            stream = connection_socket_.accept();

            if (stream != nullptr) {
                worker_ = std::make_unique<tateyama::server::stream_worker>(service_, my_session_id_, std::move(stream), database_info_);
                worker_->invoke([&]{worker_->run();});
            } else {  // connect via pipe (request_terminate)
                break;
            }
        }
    }
    void terminate() {
        connection_socket_.request_terminate();
    }

    void wait_worker_termination() {
        worker_->wait_for();
    }

private:
    tateyama::server::info_service_for_test& service_;
    tateyama::common::stream::connection_socket connection_socket_{tateyama::api::endpoint::stream::stream_client::PORT_FOR_TEST};
    std::unique_ptr<tateyama::server::stream_worker> worker_{};
    tateyama::status_info::resource::database_info_impl database_info_{"stream_info_test"};
};
}  // namespace tateyama::server


namespace tateyama::api::endpoint::stream {

class stream_info_test : public ::testing::Test {
    virtual void SetUp() {
        thread_ = std::thread(std::ref(listener_));
    }

    virtual void TearDown() {
        thread_.join();
    }

public:
    tateyama::server::info_service_for_test service_{};
    tateyama::server::stream_listener_for_test listener_{service_};
    std::thread thread_{};
};

TEST_F(stream_info_test, DISABLED_basic) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    try {
        tateyama::api::endpoint::stream::stream_client client{};
        client.send(request_test_message);
        client.receive();

        auto* request = service_.request();
        auto now = std::chrono::system_clock::now();

        // test for database_info
        auto& di = request->database_info();
        EXPECT_EQ(di.name(), "stream_info_test");
        auto d_start = di.start_at();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - d_start).count();
        EXPECT_TRUE(diff >= 500);
        EXPECT_TRUE(diff < 1500);

        // test for session_info
        auto& si = request->session_info();
        EXPECT_EQ(si.id(), my_session_id_);
        EXPECT_EQ(si.connection_type_name(), "tcp/ip");
        EXPECT_EQ(std::string(si.connection_information()).substr(0, 10), "127.0.0.1:");
        auto s_start = si.start_at();
        EXPECT_TRUE(std::chrono::duration_cast<std::chrono::milliseconds>(now - s_start).count() < 500);

        client.close();
        listener_.wait_worker_termination();
        listener_.terminate();
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

} // namespace tateyama::api::endpoint::stream
