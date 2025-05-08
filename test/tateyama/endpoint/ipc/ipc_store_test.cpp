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

#include <mutex>
#include <condition_variable>
#include <chrono>

#include "tateyama/endpoint/ipc/bootstrap/ipc_worker.h"
#include "tateyama/endpoint/header_utils.h"
#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/status/resource/database_info_impl.h"
#include "ipc_client.h"

#include <gtest/gtest.h>

namespace tateyama::server {
class ipc_listener_for_store_test {
public:
    static void run(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        worker.invoke([&]{
            worker.run();
            worker.dispose_session_store();
            worker.delete_hook();
        });
    }
    static void wait(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        while (!worker.is_terminated());
    }
};
}  // namespace tateyama::server

namespace tateyama::endpoint::ipc {

static constexpr std::size_t my_session_id = 123;

static constexpr std::string_view database_name = "ipc_store_test";
static constexpr std::string_view label = "label_fot_test";
static constexpr std::string_view application_name = "application_name_fot_test";
static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::size_t service_id_of_store_service = 102;

class store_service : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) override { return true; }
    bool start(tateyama::framework::environment&) override { return true; }
    bool shutdown(tateyama::framework::environment&) override { return true; }
    std::string_view label() const noexcept override { return __func__; }

    id_type id() const noexcept override { return service_id_of_store_service; }
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

class ipc_store_test : public ::testing::Test {
    static constexpr std::size_t writer_count = 8;

    void SetUp() override {
        rv_ = system("if [ -f /dev/shm/ipc_store_test ]; then rm -f /dev/shm/ipc_store_test; fi");
        // server part
        std::string session_name{database_name};
        session_name += "-";
        session_name += std::to_string(my_session_id);
        auto wire = std::make_unique<bootstrap::server_wire_container_impl>(session_name, "dummy_mutex_file_name", datachannel_buffer_size, 16);
        session_bridge_ = std::make_shared<session::resource::bridge>();
        const tateyama::endpoint::common::configuration conf(tateyama::endpoint::common::connection_type::ipc, session_bridge_, database_info_);
        worker_ = std::make_unique<tateyama::endpoint::ipc::bootstrap::ipc_worker>(service_, conf, my_session_id, std::move(wire));
        tateyama::server::ipc_listener_for_store_test::run(*worker_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client_ = std::make_unique<ipc_client>(database_name, my_session_id);
    }

    void TearDown() override {
        tateyama::server::ipc_listener_for_store_test::wait(*worker_);
        rv_ = system("if [ -f /dev/shm/ipc_store_test ]; then rm -f /dev/shm/ipc_store_test; fi");
    }

    int rv_;

protected:
    tateyama::status_info::resource::database_info_impl database_info_{database_name};
    store_service service_{};
    std::unique_ptr<tateyama::endpoint::ipc::bootstrap::ipc_worker> worker_{};
    std::shared_ptr<session::resource::bridge> session_bridge_{};
    std::unique_ptr<ipc_client> client_{};
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

TEST_F(ipc_store_test, basic) {
    std::shared_ptr<element_one> e1 = std::make_shared<element_one>(1);
    std::shared_ptr<element_two> e2 = std::make_shared<element_two>("test_string");
    
    // client part (send request)
    client_->send(service_id_of_store_service, std::string(request_test_message));  // we do not care service_id nor request message here

    std::string res{};
    client_->receive(res);

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

    worker_->terminate(tateyama::session::shutdown_request_type::forceful);
}

TEST_F(ipc_store_test, keep_and_dispose) {
    std::shared_ptr<element_one> e1 = std::make_shared<element_one>(1);
    std::shared_ptr<element_two> e2 = std::make_shared<element_two>("test_string");
    
    {
        // client part (send request)
        client_->send(service_id_of_store_service, std::string(request_test_message));  // we do not care service_id nor request message here

        std::string res{};
        client_->receive(res);
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
        client_->send(service_id_of_store_service, std::string(request_test_message));  // we do not care service_id nor request message here

        std::string res{};
        client_->receive(res);
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
        worker_->terminate(tateyama::session::shutdown_request_type::forceful);
        tateyama::server::ipc_listener_for_store_test::wait(*worker_);
        EXPECT_TRUE(e1->disposed());
        EXPECT_TRUE(e2->disposed());
    }
}

TEST_F(ipc_store_test, dispose_on_disconnect) {
    std::shared_ptr<element_one> e1 = std::make_shared<element_one>(1);

    {
        // client part (send request)
        client_->send(service_id_of_store_service, std::string(request_test_message));  // we do not care service_id nor request message here

        std::string res{};
        client_->receive(res);
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
    }
    tateyama::server::ipc_listener_for_store_test::wait(*worker_);
}

} // namespace tateyama::endpoint::ipc
