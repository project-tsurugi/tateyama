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
#include "tateyama/status/resource/database_info_impl.h"
#include "stream_client.h"

#include <gtest/gtest.h>

static constexpr std::size_t my_session_id_ = 123;
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::string_view response_test_message = "opqrstuvwxyz";

namespace tateyama::endpoint::stream {

class service_for_test : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }

    id_type id() const noexcept { return 100;  } // dummy
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        req_ = req;
        res_ = res;
        // do not respond to the request message in this test
        return true;
    }

private:
    std::shared_ptr<tateyama::api::server::request> req_{};
    std::shared_ptr<tateyama::api::server::response> res_{};
};

class stream_listener_for_test {
public:
    stream_listener_for_test(service_for_test& service) : service_(service) {
    }
    void operator()() {
        while (true) {
            std::shared_ptr<stream_socket> stream{};
            stream = connection_socket_.accept();

            if (stream != nullptr) {
                worker_ = std::make_unique<tateyama::endpoint::stream::bootstrap::stream_worker>(service_, my_session_id_, std::move(stream), database_info_, false);
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
        while (worker_->wait_for() != std::future_status::ready);
    }

private:
    service_for_test& service_;
    connection_socket connection_socket_{tateyama::api::endpoint::stream::stream_client::PORT_FOR_TEST};
    std::unique_ptr<tateyama::endpoint::stream::bootstrap::stream_worker> worker_{};
    tateyama::status_info::resource::database_info_impl database_info_{"stream_session_test"};
};
}


namespace tateyama::api::endpoint::stream {

class stream_session_test : public ::testing::Test {
    virtual void SetUp() {
        thread_ = std::thread(std::ref(listener_));
    }

    virtual void TearDown() {
        thread_.join();
    }

public:
    tateyama::endpoint::stream::service_for_test service_{};
    tateyama::endpoint::stream::stream_listener_for_test listener_{service_};
    std::thread thread_{};
};

TEST_F(stream_session_test, cancel) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    try {
        // client part (send request)
        auto client = std::make_unique<stream_client>();
        client->send(0, request_test_message);  // we do not care service_id nor request message here

        // client part (send cancel)
        tateyama::proto::endpoint::request::Cancel cancel{};
        tateyama::proto::endpoint::request::Request endpoint_request{};
        endpoint_request.set_allocated_cancel(&cancel);
        client->send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());
        endpoint_request.release_cancel();

        // client part (receive)
        std::string res{};
        client->receive(res, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS);
        tateyama::proto::diagnostics::Record response{};
        if(!response.ParseFromString(res)) {
            FAIL();
        }
        EXPECT_EQ(response.code(), tateyama::proto::diagnostics::Code::OPERATION_CANCELED);

        // terminate session and listener
        client->close();
        listener_.wait_worker_termination();
        listener_.terminate();
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

}
