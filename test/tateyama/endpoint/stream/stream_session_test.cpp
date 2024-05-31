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

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "tateyama/endpoint/stream/bootstrap/stream_worker.h"
#include "tateyama/endpoint/header_utils.h"
#include "tateyama/status/resource/database_info_impl.h"
#include "stream_client.h"

#include <gtest/gtest.h>

static constexpr std::size_t my_session_id = 1234;
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::size_t service_id_of_session_service = 121;

namespace tateyama::endpoint::stream {

class service_for_stream_session_test : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }

    id_type id() const noexcept { return service_id_of_session_service;  }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        req_ = req;
        res_ = res;
        // do not respond to the request message here in this test
        {
            std::unique_lock<std::mutex> lock(mtx_);
            requested_.store(true);
            condition_.notify_one();
        }
        return true;
    }

    void wait_request_arrival() {
        std::unique_lock<std::mutex> lock(mtx_);
        condition_.wait(lock, [this]{ return requested_.load(); });
    }

    void accept_cancel() {
        tateyama::proto::diagnostics::Record record{};
        record.set_code(tateyama::proto::diagnostics::Code::OPERATION_CANCELED);
        record.set_message("cancel succeeded (test message)");
        if (res_) {
            res_->error(record);
        } else {
            FAIL();
        }
    }
    void dispose_reqres() {
        req_ = nullptr;
        res_ = nullptr;
    }
    auto* get_response() {
        return res_.get();
    }
    void reply() {
        res_->body(response_test_message);
    }

private:
    std::shared_ptr<tateyama::api::server::request> req_{};
    std::shared_ptr<tateyama::api::server::response> res_{};
    std::atomic_bool requested_{};
    std::mutex mtx_{};
    std::condition_variable condition_{};
};

class stream_listener_for_session_test {
public:
    stream_listener_for_session_test(service_for_stream_session_test& service, std::shared_ptr<session::resource::bridge> session_bridge) :
        service_(service),
        session_bridge_(session_bridge) {
    }
    ~stream_listener_for_session_test() {
        connection_socket_.close();
    }
    void operator()() {
        while (true) {
            std::shared_ptr<stream_socket> stream{};
            stream = connection_socket_.accept();

            if (stream != nullptr) {
                worker_ = std::make_unique<tateyama::endpoint::stream::bootstrap::stream_worker>(service_, my_session_id, std::move(stream), database_info_, false, session_bridge_);
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
        while (!worker_->is_terminated());
    }

    auto* worker() {
        return worker_.get();
    }

private:
    service_for_stream_session_test& service_;
    std::shared_ptr<session::resource::bridge> session_bridge_;
    connection_socket connection_socket_{tateyama::api::endpoint::stream::stream_client::PORT_FOR_TEST};
    std::unique_ptr<tateyama::endpoint::stream::bootstrap::stream_worker> worker_{};
    tateyama::status_info::resource::database_info_impl database_info_{"stream_info_test"};
};
}


namespace tateyama::api::endpoint::stream {

class stream_session_test : public ::testing::Test {
    virtual void SetUp() {
        session_bridge_ = std::make_shared<session::resource::bridge>();
        listener_ = std::make_unique<tateyama::endpoint::stream::stream_listener_for_session_test>(service_, session_bridge_);
        thread_ = std::thread(std::ref(*listener_));
        client_ = std::make_unique<stream_client>();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    virtual void TearDown() {
        // terminate session
        client_->close();

        // terminate listener
        listener_->wait_worker_termination();
        listener_->terminate();

        thread_.join();
    }

public:
    std::shared_ptr<session::resource::bridge> session_bridge_{};
    std::unique_ptr<tateyama::endpoint::stream::stream_listener_for_session_test> listener_{};
    tateyama::endpoint::stream::service_for_stream_session_test service_{};
    std::thread thread_{};
    std::unique_ptr<stream_client> client_{};
};

TEST_F(stream_session_test, cancel_request_reply) {
    try {
        // client part (send request)
        EXPECT_TRUE(client_->send(service_id_of_session_service, request_test_message));  // we do not care service_id nor request message here

        // client part (send cancel)
        tateyama::proto::endpoint::request::Cancel cancel{};
        tateyama::proto::endpoint::request::Request endpoint_request{};
        endpoint_request.set_allocated_cancel(&cancel);
        EXPECT_TRUE(client_->send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString()));
        endpoint_request.release_cancel();

        service_.wait_request_arrival();
        // server part (send cancel success by error)
        service_.accept_cancel();

        // client part (receive)
        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client_->receive(res, type);
        EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS);
        tateyama::proto::diagnostics::Record response{};
        if(!response.ParseFromString(res)) {
            FAIL();
        }
        EXPECT_EQ(response.code(), tateyama::proto::diagnostics::Code::OPERATION_CANCELED);
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_session_test, cancel_request_noreply) {
    try {
        // client part (send request)
        EXPECT_TRUE(client_->send(service_id_of_session_service, request_test_message));  // we do not care service_id nor request message here

        // client part (send cancel)
        tateyama::proto::endpoint::request::Cancel cancel{};
        tateyama::proto::endpoint::request::Request endpoint_request{};
        endpoint_request.set_allocated_cancel(&cancel);
        EXPECT_TRUE(client_->send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString()));
        endpoint_request.release_cancel();

        service_.wait_request_arrival();
        // server part (send cancel success by error)
        service_.dispose_reqres();

        // client part (receive)
        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client_->receive(res, type);
        EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS);
        tateyama::proto::diagnostics::Record response{};
        if(!response.ParseFromString(res)) {
            FAIL();
        }
        EXPECT_EQ(response.code(), tateyama::proto::diagnostics::Code::UNKNOWN);
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_session_test, forceful_shutdown_after_request) {
    try {
        // client part (send request)
        EXPECT_TRUE(client_->send(service_id_of_session_service, std::string(request_test_message)));  // we do not care service_id nor request message here
        service_.wait_request_arrival();

        // shutdown request
        std::shared_ptr<tateyama::session::session_context> session_context{};
        if (auto rv = session_bridge_->session_shutdown(std::string(":") + std::to_string(my_session_id), session::shutdown_request_type::forceful, session_context); rv) {
            FAIL();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        EXPECT_FALSE(listener_->worker()->is_terminated());

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        EXPECT_TRUE(service_.get_response()->check_cancel());

        // client part (send 2nd request)
        client_->send(service_id_of_session_service, std::string(request_test_message), 1);

        // client part (receive a response to the 2nd request)
        std::string res_for_2nd{};
        tateyama::proto::framework::response::Header::PayloadType type_for_2nd{};
        client_->receive(res_for_2nd, type_for_2nd);
        EXPECT_EQ(type_for_2nd, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS);
        tateyama::proto::diagnostics::Record response{};
        if(!response.ParseFromString(res_for_2nd)) {
            FAIL();
        }
        EXPECT_EQ(response.code(), tateyama::proto::diagnostics::Code::SESSION_CLOSED);

        // server part (send dummy response message)
        service_.reply();

        // client part (receive)
        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client_->receive(res, type);
        EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
        EXPECT_EQ(res, response_test_message);

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        EXPECT_TRUE(listener_->worker()->is_terminated());
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_session_test, forceful_shutdown_before_request) {
    try {
        // shutdown request
        std::shared_ptr<tateyama::session::session_context> session_context{};
        if (auto rv = session_bridge_->session_shutdown(std::string(":") + std::to_string(my_session_id), session::shutdown_request_type::forceful, session_context); rv) {
            FAIL();
        }

        // ensure shutdown request has been processed by the worker
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        EXPECT_TRUE(listener_->worker()->is_terminated());

        // client part (send request)
        EXPECT_FALSE(client_->send(service_id_of_session_service, std::string(request_test_message)));
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_session_test, graceful_shutdown_after_request) {
    try {
        // client part (send request)
        EXPECT_TRUE(client_->send(service_id_of_session_service, std::string(request_test_message)));  // we do not care service_id nor request message here
        service_.wait_request_arrival();

        // shutdown request
        std::shared_ptr<tateyama::session::session_context> session_context{};
        if (auto rv = session_bridge_->session_shutdown(std::string(":") + std::to_string(my_session_id), session::shutdown_request_type::graceful, session_context); rv) {
            FAIL();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        EXPECT_FALSE(listener_->worker()->is_terminated());

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        EXPECT_FALSE(service_.get_response()->check_cancel());

        // client part (send 2nd request)
        client_->send(service_id_of_session_service, std::string(request_test_message), 1);

        // client part (receive a response to the 2nd request)
        std::string res_for_2nd{};
        tateyama::proto::framework::response::Header::PayloadType type_for_2nd{};
        client_->receive(res_for_2nd, type_for_2nd);
        EXPECT_EQ(type_for_2nd, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS);
        tateyama::proto::diagnostics::Record response{};
        if(!response.ParseFromString(res_for_2nd)) {
            FAIL();
        }
        EXPECT_EQ(response.code(), tateyama::proto::diagnostics::Code::SESSION_CLOSED);

        // server part (send dummy response message)
        service_.reply();

        // client part (receive)
        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client_->receive(res, type);
        EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVICE_RESULT);
        EXPECT_EQ(res, response_test_message);

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        EXPECT_TRUE(listener_->worker()->is_terminated());
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_session_test, graceful_shutdown_before_request) {
    try {
        // shutdown request
        std::shared_ptr<tateyama::session::session_context> session_context{};
        if (auto rv = session_bridge_->session_shutdown(std::string(":") + std::to_string(my_session_id), session::shutdown_request_type::graceful, session_context); rv) {
            FAIL();
        }

        // ensure shutdown request has been processed by the worker
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        EXPECT_TRUE(listener_->worker()->is_terminated());

        // client part (send request)
        EXPECT_FALSE(client_->send(service_id_of_session_service, std::string(request_test_message)));
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

}
