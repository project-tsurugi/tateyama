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
#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/endpoint/common/endpoint_proto_utils.h>

#include "tateyama/endpoint/ipc/ipc_request.h"
#include "tateyama/endpoint/ipc/ipc_response.h"

#include <tateyama/endpoint/ipc/bootstrap/server_wires_impl.h>
#include "header_utils.h"

#include <gtest/gtest.h>

namespace tateyama::endpoint::ipc {

class response_only_test : public ::testing::Test {
    static constexpr std::size_t datachannel_buffer_size = 64 * 1024;

    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/tateyama-response_only_test ]; then rm -f /dev/shm/tateyama-response_only_test; fi ");
        wire_ = std::make_shared<bootstrap::server_wire_container_impl>("tateyama-response_only_test", "dummy_mutex_file_name", datachannel_buffer_size, 16);
    }
    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/tateyama-response_only_test ]; then rm -f /dev/shm/tateyama-response_only_test; fi ");
    }

    int rv_;

public:
    static constexpr std::string_view request_test_message_ = "abcdefgh";
    static constexpr std::string_view response_test_message_ = "opqrstuvwxyz";
    static constexpr tateyama::common::wire::message_header::index_type index_ = 1;

    std::shared_ptr<bootstrap::server_wire_container_impl> wire_;

    tateyama::status_info::resource::database_info_impl dmy_dbinfo_{};
    tateyama::endpoint::common::session_info_impl dmy_ssinfo_{};

    class test_service {
    public:
        int operator()(
            std::shared_ptr<tateyama::api::server::request const> req,
            std::shared_ptr<tateyama::api::server::response> res
        ) {
            auto payload = req->payload();
            EXPECT_EQ(request_test_message_, payload);
            res->session_id(req->session_id());
            res->body(response_test_message_);
            return 0;
        }
    };

};

TEST_F(response_only_test, normal) {
    auto* request_wire = static_cast<bootstrap::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_header_content hdr{10, 100};
    std::stringstream ss{};
    append_request_header(ss, request_test_message_, hdr);
    auto request_message = ss.str();

    request_wire->write(request_message.data(), request_message.length(), index_);

    auto h = request_wire->peep(true);
    EXPECT_EQ(index_, h.get_idx());
    EXPECT_EQ(request_wire->payload(), request_message);

    auto request = std::make_shared<ipc_request>(*wire_, h, dmy_dbinfo_, dmy_ssinfo_);
    auto response = std::make_shared<ipc_response>(wire_, h.get_idx());
    EXPECT_EQ(request->session_id(), 10);
    EXPECT_EQ(request->service_id(), 100);

    test_service sv;
    sv(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
             static_cast<std::shared_ptr<tateyama::api::server::response>>(response));

    auto& response_wire = static_cast<bootstrap::server_wire_container_impl::response_wire_container_impl&>(wire_->get_response_wire());
    auto header = response_wire.await();
    std::string r_msg;
    r_msg.resize(response_wire.get_length());
    response_wire.read(r_msg.data());

    std::stringstream expected{};
    tateyama::endpoint::common::header_content hc{10};
    tateyama::endpoint::common::append_response_header(expected, response_test_message_, hc);
    EXPECT_EQ(r_msg, expected.str());
}

}  // namespace tateyama::api::endpoint::ipc
