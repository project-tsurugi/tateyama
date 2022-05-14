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
#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>
#include <tateyama/api/endpoint/service.h>

#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

#include "server_wires_impl.h"

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
            std::shared_ptr<tateyama::api::endpoint::request const> req,
            std::shared_ptr<tateyama::api::endpoint::response> res
        ) {
            auto payload = req->payload();
            EXPECT_EQ(request_test_message_, payload);

            tateyama::api::endpoint::data_channel* dc;
            EXPECT_EQ(res->acquire_channel(resultset_wire_name_, dc), tateyama::status::ok);
            res->body_head(resultset_wire_name_);

            tateyama::api::endpoint::writer* w;
            EXPECT_EQ(dc->acquire(w), tateyama::status::ok);
            w->write(r_.data(), r_.length());

            res->body(response_test_message_);
            res->code(tateyama::api::endpoint::response_code::success);
            EXPECT_EQ(dc->release(*w), tateyama::status::ok);
            EXPECT_EQ(res->release_channel(*dc), tateyama::status::ok);

            return 0;
        }
    };
};

TEST_F(result_set_test, normal) {
    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_wire->brand_new();
    const char *ptr = request_test_message_.data();
    for (std::size_t i = 0; i < request_test_message_.length(); ptr++, i++) {
        request_wire->write(*ptr);
    }
    request_wire->flush(index_);
    wire_->get_response(index_).set_query_mode();

    auto h = request_wire->peep(true);
    EXPECT_EQ(index_, h.get_idx());
    auto length = h.get_length();
    std::string message;
    message.resize(length);
    memcpy(message.data(), request_wire->payload(length), length);
    EXPECT_EQ(message, request_test_message_);

    auto request = std::make_shared<tateyama::common::wire::ipc_request>(*wire_, h);
    auto response = std::make_shared<tateyama::common::wire::ipc_response>(*request, h.get_idx());

    test_service sv;
    sv(static_cast<std::shared_ptr<tateyama::api::endpoint::request const>>(request),
             static_cast<std::shared_ptr<tateyama::api::endpoint::response>>(response));

    auto& r_box = wire_->get_response(h.get_idx());
    auto r_name = r_box.recv();
    r_box.dispose();
    EXPECT_EQ(std::string_view(r_name.first, r_name.second), resultset_wire_name_);
    auto resultset_wires =
        wire_->create_resultset_wires_for_client(std::string_view(r_name.first, r_name.second));

    auto chunk = resultset_wires->get_chunk();
    std::string r(r_);
    EXPECT_EQ(r, std::string_view(chunk.first, chunk.second));
    resultset_wires->dispose(r.length());

    auto chunk_e = resultset_wires->get_chunk();
    EXPECT_EQ(chunk_e.second, 0);
    EXPECT_TRUE(resultset_wires->is_eor());

    auto r_msg = r_box.recv();
    r_box.dispose();
    EXPECT_EQ(std::string_view(r_msg.first, r_msg.second), response_test_message_);
}

}  // namespace tateyama::api::endpoint::ipc
