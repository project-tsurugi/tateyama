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
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "tateyama/endpoint/stream/bootstrap/stream_worker.h"
#include "tateyama/endpoint/header_utils.h"
#include "tateyama/status/resource/database_info_impl.h"
#include "stream_client.h"

#include <gtest/gtest.h>

static constexpr std::size_t my_session_id_ = 1234;
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::size_t service_id_of_store_service = 123;

namespace tateyama::endpoint::stream {

class store_service_for_test : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }
    std::string_view label() const noexcept { return __func__; }

    id_type id() const noexcept { return service_id_of_store_service; }
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

class stream_listener_for_store_test {
public:
    stream_listener_for_store_test(store_service_for_test& service) : service_(service) {
    }
    ~stream_listener_for_store_test() {
        connection_socket_.close();
    }
    void operator()() {
        while (true) {
            std::shared_ptr<stream_socket> stream{};
            stream = connection_socket_.accept();

            if (stream != nullptr) {
                worker_ = std::make_unique<tateyama::endpoint::stream::bootstrap::stream_worker>(service_, my_session_id_, std::move(stream), database_info_, false);
                worker_->invoke([&]{
                    worker_->run();
                });
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
    store_service_for_test& service_;
    connection_socket connection_socket_{tateyama::api::endpoint::stream::stream_client::PORT_FOR_TEST};
    std::unique_ptr<tateyama::endpoint::stream::bootstrap::stream_worker> worker_{};
    tateyama::status_info::resource::database_info_impl database_info_{"stream_store_test"};
};
}


namespace tateyama::api::endpoint::stream {

class stream_store_test : public ::testing::Test {
    virtual void SetUp() {
        thread_ = std::thread(std::ref(listener_));
        client_ = std::make_unique<stream_client>();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    virtual void TearDown() {
        thread_.join();
    }

public:
    tateyama::endpoint::stream::store_service_for_test service_{};
    tateyama::endpoint::stream::stream_listener_for_store_test listener_{service_};
    std::thread thread_{};
    std::unique_ptr<stream_client> client_{};
};


class element_one : public tateyama::api::server::session_element {
public:
    element_one(std::size_t v) : value_(v) {}
    std::size_t value() { return value_; }
    void dispose() {
        std::unique_lock<std::mutex> lock(mutex_);
        idx_++;
        disposed_ = true;
        cond_.notify_one();
    }
    bool disposed() { return disposed_; }
    bool wait(std::size_t idx) {
        std::unique_lock<std::mutex> lk(mutex_);
        return cond_.wait_for(lk, std::chrono::seconds(3), [this, idx]{ return idx <= idx_; });
    }
    std::size_t idx() { return idx_; }
private:
    std::size_t value_;
    bool disposed_{false};
    std::size_t idx_{};
    std::mutex mutex_{};
    std::condition_variable cond_{};
};

class element_two : public tateyama::api::server::session_element {
public:
    element_two(std::string v) : value_(v) {}
    std::string value() { return value_; }
    void dispose() { disposed_ = true; }
    bool disposed() { return disposed_; }
private:
    std::string value_;
    bool disposed_{false};
};

TEST_F(stream_store_test, basic) {
    try {
        std::shared_ptr<element_one> e1 = std::make_shared<element_one>(1);
        std::shared_ptr<element_two> e2 = std::make_shared<element_two>("test_string");
    
        // client part (send request)
        client_->send(service_id_of_store_service, request_test_message);  // we do not care service_id nor request message here

        // client part (receive)
        std::string res{};
        tateyama::proto::framework::response::Header::PayloadType type{};
        client_->receive(res, type);

        auto* req = service_.request();
        auto& store = req->session_store();

        // put
        EXPECT_TRUE(store.put<element_one>(1, e1));
        EXPECT_TRUE(store.put<element_two>(2, e2));
        EXPECT_FALSE(store.put<element_one>(1, e1));
        EXPECT_FALSE(store.put<element_two>(2, e2));

        // find
        EXPECT_EQ(store.find<element_one>(1), e1);
        EXPECT_EQ(store.find<element_two>(2), e2);

        EXPECT_EQ(store.find<element_one>(2), nullptr);
        EXPECT_EQ(store.find<element_two>(1), nullptr);

        // find_or_emplace
        EXPECT_EQ(store.find_or_emplace<element_two>(1, "test_2"), nullptr);
        EXPECT_EQ(store.find_or_emplace<element_one>(2, 100), nullptr);

        auto e1d = store.find_or_emplace<element_one>(1, 100);
        auto e2d = store.find_or_emplace<element_two>(2, "test_2");
        EXPECT_EQ(e1, e1d);
        EXPECT_EQ(e2, e2d);

        EXPECT_EQ(store.find_or_emplace<element_one>(3, 100), nullptr);
        EXPECT_EQ(store.find_or_emplace<element_two>(4, "test_2"), nullptr);

        // remove
        EXPECT_FALSE(store.remove<element_two>(1));
        EXPECT_FALSE(store.remove<element_one>(2));
        EXPECT_EQ(store.find<element_one>(1), e1);
        EXPECT_EQ(store.find<element_two>(2), e2);

        EXPECT_TRUE(store.remove<element_one>(1));
        EXPECT_TRUE(store.remove<element_two>(2));
        EXPECT_EQ(store.find<element_one>(1), nullptr);
        EXPECT_EQ(store.find<element_two>(2), nullptr);

        client_->close();
        listener_.wait_worker_termination();
        listener_.terminate();
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_store_test, keep_and_dispose) {
    try {
        std::shared_ptr<element_one> e1 = std::make_shared<element_one>(1);
        std::shared_ptr<element_two> e2 = std::make_shared<element_two>("test_string");
    
        {
            // client part (send request)
            client_->send(service_id_of_store_service, request_test_message);  // we do not care service_id nor request message here

            // client part (receive)
            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client_->receive(res, type);
        }

        {
            auto* req = service_.request();
            auto& store = req->session_store();

            // put
            EXPECT_TRUE(store.put<element_one>(1, e1));
            EXPECT_TRUE(store.put<element_two>(2, e2));
        }

        {
            // client part (send request)
            client_->send(service_id_of_store_service, request_test_message);  // we do not care service_id nor request message here

            // client part (receive)
            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client_->receive(res, type);
        }

        {
            auto* req = service_.request();
            auto& store = req->session_store();

            // find
            EXPECT_EQ(store.find<element_one>(1), e1);
            EXPECT_EQ(store.find<element_two>(2), e2);
        }

        {
            EXPECT_FALSE(e1->disposed());
            EXPECT_FALSE(e2->disposed());
            client_->close();
            listener_.wait_worker_termination();
            listener_.terminate();
            EXPECT_TRUE(e1->disposed());
            EXPECT_TRUE(e2->disposed());
        }
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

TEST_F(stream_store_test, dispose_on_disconnect) {
    try {
        std::shared_ptr<element_one> e1 = std::make_shared<element_one>(1);

        {
            // client part (send request)
            client_->send(service_id_of_store_service, request_test_message);  // we do not care service_id nor request message here

            // client part (receive)
            std::string res{};
            tateyama::proto::framework::response::Header::PayloadType type{};
            client_->receive(res, type);
        }

        {
            auto* req = service_.request();
            auto& store = req->session_store();

            // put
            EXPECT_TRUE(store.put<element_one>(1, e1));
            auto idx = e1->idx();

            client_->disconnect();
            EXPECT_TRUE(e1->wait(idx + 1));
            EXPECT_TRUE(e1->disposed());

            client_->close();
            listener_.wait_worker_termination();
            listener_.terminate();
        }
    } catch (std::runtime_error &ex) {
        std::cout << ex.what() << std::endl;
        FAIL();
    }
}

}
