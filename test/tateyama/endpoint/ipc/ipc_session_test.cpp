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

#include "tateyama/endpoint/ipc/bootstrap/worker.h"
#include "tateyama/endpoint/header_utils.h"
#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/status/resource/database_info_impl.h"
#include <tateyama/session/resource/bridge.h>
#include "ipc_client.h"

#include <gtest/gtest.h>

namespace tateyama::server {
class ipc_listener_for_session_test {
public:
    static void run(tateyama::endpoint::ipc::bootstrap::Worker& worker) {
        worker.invoke([&]{worker.run();});
    }
    static void wait(tateyama::endpoint::ipc::bootstrap::Worker& worker) {
        while (worker.wait_for() != std::future_status::ready);
    }
};
}  // namespace tateyama::server

namespace tateyama::endpoint::ipc {

static constexpr std::size_t my_session_id = 123;

static constexpr std::string_view database_name = "ipc_sessionsession_test";
static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::string_view response_test_message = "opqrstuvwxyz";

class session_service : public tateyama::framework::routing_service {
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
        requested_ = true;
        return true;
    }

    bool requested() const {
        return requested_;
    }

private:
    std::shared_ptr<tateyama::api::server::request> req_{};
    std::shared_ptr<tateyama::api::server::response> res_{};
    bool requested_{};
};

class ipc_session_test : public ::testing::Test {
    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/ipc_session_test ]; then rm -f /dev/shm/ipc_session_test; fi");

        // server part
        std::string session_name{database_name};
        session_name += "-";
        session_name += std::to_string(my_session_id);
        auto wire = std::make_shared<bootstrap::server_wire_container_impl>(session_name, "dummy_mutex_file_name", datachannel_buffer_size, 16);
        session_bridge_ = std::make_shared<session::resource::bridge>();
        worker_ = std::make_unique<tateyama::endpoint::ipc::bootstrap::Worker>(service_, my_session_id, wire, database_info_, session_bridge_);
        tateyama::server::ipc_listener_for_session_test::run(*worker_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client_ = std::make_unique<ipc_client>(database_name, my_session_id);
    }

    virtual void TearDown() {
        tateyama::server::ipc_listener_for_session_test::wait(*worker_);

        rv_ = system("if [ -f /dev/shm/ipc_session_test ]; then rm -f /dev/shm/ipc_session_test; fi");
    }

    int rv_;

protected:
    tateyama::status_info::resource::database_info_impl database_info_{database_name};
    session_service service_{};
    std::unique_ptr<tateyama::endpoint::ipc::bootstrap::Worker> worker_{};
    std::shared_ptr<session::resource::bridge> session_bridge_{};
    std::unique_ptr<ipc_client> client_{};
};

TEST_F(ipc_session_test, cancel_request) {
    // client part (send request)
    client_->send(0, std::string(request_test_message));  // we do not care service_id nor request message here

    // client part (send cancel)
    tateyama::proto::endpoint::request::Cancel cancel{};
    tateyama::proto::endpoint::request::Request endpoint_request{};
    endpoint_request.set_allocated_cancel(&cancel);
    client_->send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());
    endpoint_request.release_cancel();

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

    worker_->terminate();
}

TEST_F(ipc_session_test, shutdown_after_request) {
    // client part (send request)
    client_->send(0, std::string(request_test_message));  // we do not care service_id nor request message here
    while (!service_.requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // shutdown request
    std::shared_ptr<tateyama::session::session_context> session_context{};
    session_bridge_->session_shutdown(std::string(":") + std::to_string(my_session_id), session::shutdown_request_type::forceful, session_context);

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
}

TEST_F(ipc_session_test, shutdown_before_request) {
    // shutdown request
    std::shared_ptr<tateyama::session::session_context> session_context{};
    session_bridge_->session_shutdown(std::string(":") + std::to_string(my_session_id), session::shutdown_request_type::forceful, session_context);

    // ensure shutdown request has been processed by the worker
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // client part (send request)
    client_->send(0, std::string(request_test_message));  // we do not care service_id nor request message here

    // client part (receive)
    std::string res{};
    tateyama::proto::framework::response::Header::PayloadType type{};

    EXPECT_THROW(client_->receive(res, type), std::runtime_error);
}

} // namespace tateyama::endpoint::ipc
