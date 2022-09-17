/*
 * Copyright 2018-2021 tsurugi project.
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
#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

#include "server_wires_test.h"
#include "header_utils.h"

#include <gtest/gtest.h>

namespace tateyama::api::endpoint::ipc {

class result_set_test : public ::testing::Test {
    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/tateyama-test ]; then rm -f /dev/shm/tateyama-test; fi ");
        wire_ = std::make_unique<tateyama::common::wire::server_wire_container_impl>("tateyama-test");
    }
    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/tateyama-test ]; then rm -f /dev/shm/tateyama-test*; fi ");
    }

    int rv_;

public:
    static constexpr std::string_view request_test_message_ = "abcdefgh";
    static constexpr std::string_view response_test_message_ = "opqrstuvwxyz";
    static constexpr std::string_view resultset_wire_name_ = "resultset_1";
    static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
    static constexpr std::string_view r_ = "row_data";

    std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire_;

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

            res->body(response_test_message_);
            res->code(tateyama::api::server::response_code::success);
            EXPECT_EQ(dc->release(*w), tateyama::status::ok);
            EXPECT_EQ(res->release_channel(*dc), tateyama::status::ok);

            return 0;
        }
    };
};

TEST_F(result_set_test, normal) {
    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->brand_new();
    const char *ptr = request_message.data();
    for (std::size_t i = 0; i < request_message.length(); ptr++, i++) {
        request_wire->write(*ptr);
    }
    request_wire->flush(index_);

    auto h = request_wire->peep(true);
    EXPECT_EQ(index_, h.get_idx());
    auto length = h.get_length();
    std::string message;
    message.resize(length);
    memcpy(message.data(), request_wire->payload(length), length);
    EXPECT_EQ(message, request_message);

    auto request = std::make_shared<tateyama::common::wire::ipc_request>(*wire_, h);
    auto response = std::make_shared<tateyama::common::wire::ipc_response>(*request, h.get_idx());

    test_service sv;
    sv(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
             static_cast<std::shared_ptr<tateyama::api::server::response>>(response));

    auto& response_wire = wire_->get_response_wire();
    auto header_1st = response_wire.await();
    
    std::stringstream expected_resultset_wire_name{};
    tateyama::endpoint::common::header_content hc{};
    tateyama::endpoint::common::append_response_header(expected_resultset_wire_name, resultset_wire_name_, hc);
    EXPECT_EQ(std::string_view(r_name.first, r_name.second), expected_resultset_wire_name.str());
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

    std::stringstream expected{};
    tateyama::endpoint::common::header_content hc2{};
    tateyama::endpoint::common::append_response_header(expected, response_test_message_, hc2);
    EXPECT_EQ(std::string_view(r_msg.first, r_msg.second), expected.str());
}

}  // namespace tateyama::api::endpoint::ipc
