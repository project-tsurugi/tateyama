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
#include "tateyama/status/resource/database_info_impl.h"

#include <gtest/gtest.h>

namespace tateyama::server {
class ipc_listener_for_test {
public:
    static void run(tateyama::server::Worker& worker) {
        worker.invoke([&]{worker.run();});
    }
    static void wait(tateyama::server::Worker& worker) {
        worker.wait_for();
    }

};
}  // namespace tateyama::server

namespace tateyama::api::endpoint::ipc {

static constexpr std::size_t my_session_id_ = 123;

static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::string_view request_test_message_ = "abcdefgh";

class info_service : public tateyama::framework::routing_service {
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

class ipc_info_test : public ::testing::Test {
    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/ipc_info_test ]; then rm -f /dev/shm/ipc_info_test; fi");
    }

    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/ipc_info_test ]; then rm -f /dev/shm/ipc_info_test; fi");
    }

    int rv_;

public:
    tateyama::status_info::resource::database_info_impl database_info_{"ipc_info_test"};
    info_service service_{};
};

TEST_F(ipc_info_test, DISABLED_basic) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto wire = std::make_shared<tateyama::common::wire::server_wire_container_impl>("ipc_info_test", "dummy_mutex_file_name", datachannel_buffer_size, 16);
    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire->get_request_wire());
    auto& response_wire = dynamic_cast<tateyama::common::wire::server_wire_container_impl::response_wire_container_impl&>(wire->get_response_wire());
    tateyama::server::Worker worker(service_, my_session_id_, wire, [](){}, database_info_);
    tateyama::server::ipc_listener_for_test::run(worker);

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();
    request_wire->write(request_message.data(), request_message.length(), index_);
    response_wire.await();

    auto* request = service_.request();
    auto now = std::chrono::system_clock::now();

    // test for database_info
    auto& di = request->database_info();
    EXPECT_EQ(di.name(), "ipc_info_test");
    auto d_start = di.start_at();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - d_start).count();
    EXPECT_TRUE(diff >= 500);
    EXPECT_TRUE(diff < 1500);

    // test for session_info
    auto& si = request->session_info();
    EXPECT_EQ(si.id(), my_session_id_);
    EXPECT_EQ(si.connection_type_name(), "ipc");
    auto s_start = si.start_at();
    EXPECT_TRUE(std::chrono::duration_cast<std::chrono::milliseconds>(now - s_start).count() < 500);

    worker.terminate();
    tateyama::server::ipc_listener_for_test::wait(worker);
}

} // namespace tateyama::api::endpoint::ipc
