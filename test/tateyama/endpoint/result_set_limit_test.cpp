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
#include <stdlib.h>
#include <future>
#include <thread>
#include <functional>
#include <array>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

#include <tateyama/endpoint/ipc/bootstrap/server_wires_impl.h>
#include "header_utils.h"

#include <gtest/gtest.h>

namespace tateyama::endpoint::ipc {

class result_set_limit_test : public ::testing::Test {
    virtual void SetUp() {

        rv_ = system("if [ -f /dev/shm/tateyama-result_set_limit_test ]; then rm -f /dev/shm/tateyama-result_set_limit_test; fi ");

        wire_ = std::make_shared<bootstrap::server_wire_container_impl>("tateyama-result_set_limit_test", "dummy_mutex_file_name", datachannel_buffer_size, writers);
        buffer.resize(value_size);
    }
    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/tateyama-result_set_limit_test ]; then rm -f /dev/shm/tateyama-result_set_limit_test*; fi ");
    }

    int rv_;

public:
    static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
    static constexpr std::size_t value_size = 1024;
    static constexpr std::size_t writers = 16;

    static constexpr std::string_view request_test_message_ = "abcdefgh";
    static constexpr std::string_view response_test_message_ = "opqrstuvwxyz";
    static constexpr std::string_view resultset_wire_name_ = "resultset_1";
    static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
    std::string buffer{};

    std::shared_ptr<bootstrap::server_wire_container_impl> wire_;

    tateyama::status_info::resource::database_info_impl dmy_dbinfo_{};
    tateyama::endpoint::common::session_info_impl dmy_ssinfo_{};

    bool timeout(std::function<void()>&& test_behavior) {
        std::promise<bool> promisedFinished;
        auto futureResult = promisedFinished.get_future();
        std::thread([](std::promise<bool>& finished, std::function<void()>&& tf) {
            tf();
            finished.set_value(true);
        }, std::ref(promisedFinished), test_behavior).detach();
  
        return futureResult.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout;
    }
};

TEST_F(result_set_limit_test, within_datachannel_buffer_size) {
    auto* request_wire = static_cast<bootstrap::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->write(request_message.data(), request_message.length(), index_);

    auto h = request_wire->peep(true);
    EXPECT_EQ(index_, h.get_idx());
    EXPECT_EQ(request_wire->payload(), request_message);

    auto request = std::make_shared<ipc_request>(*wire_, h, dmy_dbinfo_, dmy_ssinfo_);
    auto response = std::make_shared<ipc_response>(wire_, h.get_idx());

    std::shared_ptr<tateyama::api::server::data_channel> dc;
    EXPECT_EQ(response->acquire_channel(resultset_wire_name_, dc), tateyama::status::ok);

    std::shared_ptr<tateyama::api::server::writer> w;
    EXPECT_EQ(dc->acquire(w), tateyama::status::ok);

    for (auto i = 0; i < (datachannel_buffer_size / value_size - 1); i++) {
        EXPECT_EQ(tateyama::status::ok, w->write(buffer.data(), buffer.length()));
    }
}

TEST_F(result_set_limit_test, exceed_datachannel_buffer_size) {
    auto* request_wire = static_cast<bootstrap::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->write(request_message.data(), request_message.length(), index_);

    auto h = request_wire->peep(true);
    EXPECT_EQ(index_, h.get_idx());
    EXPECT_EQ(request_wire->payload(), request_message);

    auto request = std::make_shared<ipc_request>(*wire_, h, dmy_dbinfo_, dmy_ssinfo_);
    auto response = std::make_shared<ipc_response>(wire_, h.get_idx());

    std::shared_ptr<tateyama::api::server::data_channel> dc;
    EXPECT_EQ(response->acquire_channel(resultset_wire_name_, dc), tateyama::status::ok);

    std::shared_ptr<tateyama::api::server::writer> w;
    EXPECT_EQ(dc->acquire(w), tateyama::status::ok);

    for (auto i = 0; i < (datachannel_buffer_size / value_size - 1); i++) {
        EXPECT_EQ(tateyama::status::ok, w->write(buffer.data(), buffer.length()));
    }
    EXPECT_NE(tateyama::status::ok, w->write(buffer.data(), buffer.length()));
}

TEST_F(result_set_limit_test, within_writers) {
    auto* request_wire = static_cast<bootstrap::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->write(request_message.data(), request_message.length(), index_);

    auto h = request_wire->peep(true);
    EXPECT_EQ(index_, h.get_idx());
    EXPECT_EQ(request_wire->payload(), request_message);

    auto request = std::make_shared<ipc_request>(*wire_, h, dmy_dbinfo_, dmy_ssinfo_);
    auto response = std::make_shared<ipc_response>(wire_, h.get_idx());

    std::array<std::shared_ptr<tateyama::api::server::data_channel>, writers+1> dcs{};
    std::array<std::shared_ptr<tateyama::api::server::writer>, writers+1> ws{};
    
    for (auto i = 0; i < writers; i++) {
        std::shared_ptr<tateyama::api::server::data_channel> dc;
        std::string rn{resultset_wire_name_};
        rn += std::to_string(i);
        EXPECT_EQ(response->acquire_channel(rn, dc), tateyama::status::ok);
        dcs.at(i) = std::move(dc);

        std::shared_ptr<tateyama::api::server::writer> w;
        EXPECT_EQ(dcs.at(i)->acquire(w), tateyama::status::ok);
        ws.at(i) = std::move(w);
    }
}

}  // namespace tateyama::api::endpoint::ipc
