/*
 * Copyright 2018-2023 tsurugi project.
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
#include <gtest/gtest.h>

#include "ipc_client.h"

namespace tateyama::api::endpoint::ipc {

ipc_client::ipc_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg) {
    get_ipc_database_name(cfg, database_name_);
    container_ = std::make_unique<tsubakuro::common::wire::connection_container>(database_name_);
    id_ = container_->get_connection_queue().request();
    session_id_ = container_->get_connection_queue().wait(id_);
    //
    session_name_ = database_name_ + "-" + std::to_string(session_id_);
    swc_ = std::make_unique<tsubakuro::common::wire::session_wire_container>(session_name_);
    request_wire_ = &swc_->get_request_wire();
    response_wire_ = &swc_->get_response_wire();
}

static constexpr tsubakuro::common::wire::message_header::index_type ipc_test_index = 1234;

void ipc_client::send(const std::size_t tag, const std::string &message) {
    request_header_content hdr { session_id_, tag };
    std::stringstream ss { };
    append_request_header(ss, message, hdr);
    auto request_message = ss.str();
    request_wire_->write(reinterpret_cast<const signed char*>(request_message.data()), request_message.length(),
            ipc_test_index);
}

/*
 *  see parse_header() in tateyama/endpoint/common/endpoint_proto_utils.h
 */
struct parse_response_result {
    std::size_t session_id_ { };
    std::string_view payload_ { };
};

bool parse_response_header(std::string_view input, parse_response_result &result) {
    result = { };
    ::tateyama::proto::framework::response::Header hdr { };
    google::protobuf::io::ArrayInputStream in { input.data(), static_cast<int>(input.size()) };
    if (auto res = utils::ParseDelimitedFromZeroCopyStream(std::addressof(hdr), std::addressof(in), nullptr); !res) {
        return false;
    }
    result.session_id_ = hdr.session_id();
    return utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, result.payload_);
}

void ipc_client::receive(std::string &message) {
    tsubakuro::common::wire::response_header header = response_wire_->await();
    EXPECT_EQ(ipc_test_index, header.get_idx());
    ASSERT_GT(header.get_length(), 0);
    EXPECT_EQ(1, header.get_type());
    std::string r_msg;
    r_msg.resize(header.get_length());
    response_wire_->read(reinterpret_cast<signed char*>(r_msg.data()));
    //
    parse_response_result result;
    ASSERT_TRUE(parse_response_header(r_msg, result));
    EXPECT_EQ(session_id_, result.session_id_);
    message = result.payload_;
}

resultset_wires_container *ipc_client::create_resultset_wires() {
    return swc_->create_resultset_wire();
}

void ipc_client::dispose_resultset_wires(resultset_wires_container *rwc) {
    swc_->dispose_resultset_wire(rwc);
}

} // namespace tateyama::api::endpoint::ipc