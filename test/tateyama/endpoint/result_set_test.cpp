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

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

#include <tateyama/endpoint/ipc/bootstrap/server_wires_impl.h>
#include "header_utils.h"

#include <gtest/gtest.h>

namespace tateyama::endpoint::ipc {

class result_set_test : public ::testing::Test {
    static constexpr std::size_t datachannel_buffer_size = 64 * 1024;

    virtual void SetUp() {

        rv_ = system("if [ -f /dev/shm/tateyama-result_set_test ]; then rm -f /dev/shm/tateyama-result_set_test; fi ");

        wire_ = std::make_shared<bootstrap::server_wire_container_impl>("tateyama-result_set_test", "dummy_mutex_file_name", datachannel_buffer_size, 16);

    }
    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/tateyama-result_set_test ]; then rm -f /dev/shm/tateyama-result_set_test*; fi ");
    }

    int rv_;

public:
    static constexpr std::string_view request_test_message_ = "abcdefgh";
    static constexpr std::string_view response_test_message_ = "opqrstuvwxyz";
    static constexpr std::string_view resultset_wire_name_ = "resultset_1";
    static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
    static constexpr std::string_view r_ = "row_data_test";  // length = 13
    static constexpr std::size_t writer_count = 8;

    std::shared_ptr<bootstrap::server_wire_container_impl> wire_;

    tateyama::status_info::resource::database_info_impl dmy_dbinfo_{};
    tateyama::endpoint::common::session_info_impl dmy_ssinfo_{};
    tateyama::api::server::session_store dmy_ssstore_{};
    tateyama::session::session_variable_set dmy_svariable_set_{};

    class test_service {
    public:
        int operator()(
            std::shared_ptr<tateyama::api::server::request const> req,
            std::shared_ptr<tateyama::api::server::response> res
        ) {
            auto payload = req->payload();
            EXPECT_EQ(request_test_message_, payload);

            std::shared_ptr<tateyama::api::server::data_channel> dc;
            EXPECT_EQ(res->acquire_channel(resultset_wire_name_, dc), tateyama::status::ok);
            res->body_head(resultset_wire_name_);

            std::shared_ptr<tateyama::api::server::writer> w;
            EXPECT_EQ(dc->acquire(w), tateyama::status::ok);
            w->write(r_.data(), r_.length());
            w->commit();

            EXPECT_EQ(dc->release(*w), tateyama::status::ok);
            EXPECT_EQ(res->release_channel(*dc), tateyama::status::ok);
            res->body(response_test_message_);

            return 0;
        }
    };
};

TEST_F(result_set_test, normal) {
    auto* request_wire = static_cast<bootstrap::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->write(request_message.data(), request_message.length(), index_);

    auto h = request_wire->peep();
    EXPECT_EQ(index_, h.get_idx());
    EXPECT_EQ(request_wire->payload(), request_message);

    auto request = std::make_shared<ipc_request>(*wire_, h, dmy_dbinfo_, dmy_ssinfo_, dmy_ssstore_, dmy_svariable_set_);
    auto response = std::make_shared<ipc_response>(wire_, h.get_idx(), writer_count, [](){});

    test_service sv;
    sv(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
             static_cast<std::shared_ptr<tateyama::api::server::response>>(response));

    auto& response_wire = static_cast<bootstrap::server_wire_container_impl::response_wire_container_impl&>(wire_->get_response_wire());
    auto header_1st = response_wire.await();
    std::string r_name;
    r_name.resize(header_1st.get_length());
    response_wire.read(r_name.data());

    std::stringstream expected_resultset_wire_name{};
    tateyama::endpoint::common::header_content hc{};
    tateyama::endpoint::common::append_response_header(expected_resultset_wire_name, resultset_wire_name_, hc, ::tateyama::proto::framework::response::Header::SERVICE_RESULT);
    EXPECT_EQ(r_name, expected_resultset_wire_name.str());
    auto resultset_wires =
        wire_->create_resultset_wires_for_client(resultset_wire_name_);

    auto chunk = resultset_wires->get_chunk();
    ASSERT_NE(chunk.data(), nullptr);
    std::string r(r_);
    EXPECT_EQ(r, chunk);
    resultset_wires->dispose(r.length());

    auto chunk_e = resultset_wires->get_chunk();
    EXPECT_EQ(chunk_e.length(), 0);
    EXPECT_TRUE(resultset_wires->is_eor());

    auto header_2nd = response_wire.await();
    std::string r_msg;
    r_msg.resize(header_2nd.get_length());
    response_wire.read(r_msg.data());

    std::stringstream expected{};
    tateyama::endpoint::common::header_content hc2{};
    tateyama::endpoint::common::append_response_header(expected, response_test_message_, hc2, ::tateyama::proto::framework::response::Header::SERVICE_RESULT);
    EXPECT_EQ(r_msg, expected.str());
}

TEST_F(result_set_test, large) {
    static constexpr std::size_t loop_count = 4096;

    auto* request_wire = static_cast<bootstrap::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->write(request_message.data(), request_message.length(), index_);

    auto h = request_wire->peep();
    EXPECT_EQ(index_, h.get_idx());
    EXPECT_EQ(request_wire->payload(), request_message);

    auto request = std::make_shared<ipc_request>(*wire_, h, dmy_dbinfo_, dmy_ssinfo_, dmy_ssstore_, dmy_svariable_set_);
    auto response = std::make_shared<ipc_response>(wire_, h.get_idx(), writer_count, [](){});


    // server side
    auto req = static_cast<std::shared_ptr<tateyama::api::server::request const>>(request);
    auto res = static_cast<std::shared_ptr<tateyama::api::server::response>>(response);

    auto payload = req->payload();
    EXPECT_EQ(request_test_message_, payload);

    std::shared_ptr<tateyama::api::server::data_channel> dc;
    EXPECT_EQ(res->acquire_channel(resultset_wire_name_, dc), tateyama::status::ok);
    res->body_head(resultset_wire_name_);

    std::shared_ptr<tateyama::api::server::writer> w;
    EXPECT_EQ(dc->acquire(w), tateyama::status::ok);
    for (std::size_t i = 0; i < loop_count; i++) {
        w->write(r_.data(), r_.length());
        w->commit();
    }

    EXPECT_EQ(dc->release(*w), tateyama::status::ok);
    EXPECT_EQ(res->release_channel(*dc), tateyama::status::ok);
    res->body(response_test_message_);


    // client side
    auto& response_wire = dynamic_cast<bootstrap::server_wire_container_impl::response_wire_container_impl&>(wire_->get_response_wire());
    auto header_1st = response_wire.await();
    std::string r_name;
    r_name.resize(header_1st.get_length());
    response_wire.read(r_name.data());

    std::stringstream expected_resultset_wire_name{};
    tateyama::endpoint::common::header_content hc{};
    tateyama::endpoint::common::append_response_header(expected_resultset_wire_name, resultset_wire_name_, hc, ::tateyama::proto::framework::response::Header::SERVICE_RESULT);
    EXPECT_EQ(r_name, expected_resultset_wire_name.str());
    auto resultset_wires =
        wire_->create_resultset_wires_for_client(resultset_wire_name_);

    for (std::size_t i = 0; i < loop_count; i++) {
        auto chunk = resultset_wires->get_chunk();
        ASSERT_NE(chunk.data(), nullptr);
        std::string r(r_);
        EXPECT_EQ(r, chunk);
        resultset_wires->dispose(r.length());
    }

    auto chunk_e = resultset_wires->get_chunk();
    EXPECT_EQ(chunk_e.length(), 0);
    EXPECT_TRUE(resultset_wires->is_eor());

    auto header_2nd = response_wire.await();
    std::string r_msg;
    r_msg.resize(header_2nd.get_length());
    response_wire.read(r_msg.data());

    std::stringstream expected{};
    tateyama::endpoint::common::header_content hc2{};
    tateyama::endpoint::common::append_response_header(expected, response_test_message_, hc2, ::tateyama::proto::framework::response::Header::SERVICE_RESULT);
    EXPECT_EQ(r_msg, expected.str());
}

}  // namespace tateyama::api::endpoint::ipc
