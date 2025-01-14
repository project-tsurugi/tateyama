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

#include "tateyama/endpoint/ipc/bootstrap/ipc_worker.h"
#include "tateyama/endpoint/header_utils.h"
#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/status/resource/database_info_impl.h"
#include "ipc_client.h"

#include <gtest/gtest.h>

namespace tateyama::server {
class ipc_listener_for_test {
public:
    static void run(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        worker.invoke([&]{
            worker.run();
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

static constexpr std::string_view database_name = "ipc_info_test";
static constexpr std::string_view label = "label_fot_test";
static constexpr std::string_view application_name = "application_name_fot_test";
static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::size_t service_id_of_info_service = 101;

class info_service : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }
    std::string_view label() const noexcept { return __func__; }

    id_type id() const noexcept { return service_id_of_info_service; }
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

class ipc_info_test : public ::testing::Test {
    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/ipc_info_test ]; then rm -f /dev/shm/ipc_info_test; fi");
    }

    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/ipc_info_test ]; then rm -f /dev/shm/ipc_info_test; fi");
    }

    int rv_;

public:
    tateyama::status_info::resource::database_info_impl database_info_{database_name};
    info_service service_{};
};

static constexpr std::size_t writer_count = 8;

TEST_F(ipc_info_test, basic) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // server part
    std::string session_name{database_name};
    session_name += "-";
    session_name += std::to_string(my_session_id);
    auto wire = std::make_shared<bootstrap::server_wire_container_impl>(session_name, "dummy_mutex_file_name", datachannel_buffer_size, 16);
    const tateyama::endpoint::common::worker_common::configuration conf(tateyama::endpoint::common::worker_common::connection_type::ipc);
    tateyama::endpoint::ipc::bootstrap::ipc_worker worker(service_, conf, my_session_id, wire, database_info_);
    tateyama::server::ipc_listener_for_test::run(worker);

    // client part
    tateyama::proto::endpoint::request::ClientInformation cci{};
    cci.set_connection_label(std::string(label));
    cci.set_application_name(std::string(application_name));
    tateyama::proto::endpoint::request::Credential cred{};
    // FIXME handle userName when a credential specification is fixed.
    cci.set_allocated_credential(&cred);
    tateyama::proto::endpoint::request::Handshake hs{};
    hs.set_allocated_client_information(&cci);
    auto client = std::make_unique<ipc_client>(database_name, my_session_id, hs);
    cci.release_credential();
    hs.release_client_information();

    client->send(service_id_of_info_service, std::string(request_test_message));  // we do not care service_id nor request message here
    std::string res{};
    client->receive(res);
    
    // server part
    auto* request = service_.request();
    auto now = std::chrono::system_clock::now();

    // test for database_info
    auto& di = request->database_info();
    EXPECT_EQ(di.name(), database_name);
    auto d_start = di.start_at();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - d_start).count();
    EXPECT_TRUE(diff >= 500);
    EXPECT_TRUE(diff < 1500);

    // test for session_info
    auto& si = request->session_info();
    EXPECT_EQ(si.label(), label);
    EXPECT_EQ(si.application_name(), application_name);
    EXPECT_EQ(si.id(), my_session_id);
    EXPECT_EQ(si.connection_type_name(), "ipc");
    auto s_start = si.start_at();
    EXPECT_TRUE(std::chrono::duration_cast<std::chrono::milliseconds>(now - s_start).count() < 500);

    worker.terminate(tateyama::session::shutdown_request_type::forceful);
    tateyama::server::ipc_listener_for_test::wait(worker);
}

} // namespace tateyama::endpoint::ipc
