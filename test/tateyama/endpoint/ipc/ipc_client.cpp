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
#include "ipc_client.h"

#include <gtest/gtest.h>

namespace tateyama::endpoint::ipc {

ipc_client::ipc_client(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, tateyama::proto::endpoint::request::Handshake& hs)
    : endpoint_handshake_(hs) {
    get_ipc_database_name(cfg, database_name_);
    container_ = std::make_unique < tsubakuro::common::wire::connection_container > (database_name_);
    id_ = container_->get_connection_queue().request();
    session_id_ = container_->get_connection_queue().wait(id_);
    //
    session_name_ = database_name_ + "-" + std::to_string(session_id_);
    swc_ = std::make_unique < tsubakuro::common::wire::session_wire_container > (session_name_);
    request_wire_ = &swc_->get_request_wire();
    response_wire_ = &swc_->get_response_wire();

    handshake();
}

ipc_client::ipc_client(std::string_view name, std::size_t session_id, tateyama::proto::endpoint::request::Handshake& hs)
    : endpoint_handshake_(hs), database_name_(name), session_id_(session_id) {

    session_name_ = database_name_ + "-" + std::to_string(session_id_);
    swc_ = std::make_unique < tsubakuro::common::wire::session_wire_container > (session_name_);
    request_wire_ = &swc_->get_request_wire();
    response_wire_ = &swc_->get_response_wire();

    handshake();
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
    tateyama::proto::framework::response::Header::PayloadType payload_type_{};
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
    result.payload_type_ = hdr.payload_type();
    return utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, result.payload_);
}

void ipc_client::receive(std::string &message) {
    receive(message, static_cast<tateyama::proto::framework::response::Header::PayloadType>(0), false);
}
void ipc_client::receive(std::string &message, tateyama::proto::framework::response::Header::PayloadType type) {
    receive(message, type, true);
}

void ipc_client::receive(std::string &message, tateyama::proto::framework::response::Header::PayloadType type, bool do_check) {
    tsubakuro::common::wire::response_header header;
    int ntry = 0;
    bool ok = false;
    do {
        // NOTE: await() throws exception if it cannot receive any response in a few seconds.
        try {
            header = response_wire_->await();
            ok = true;
        } catch (const std::runtime_error &ex) {
            std::cout << ex.what() << std::endl;
            ntry++;
            if (ntry >= 100) {
                assert_failed();
            }
        }
    } while (!ok);
    // EXPECT_EQ(ipc_test_index, header.get_idx());
    // ASSERT_GT(header.get_length(), 0);
    std::string r_msg;
    r_msg.resize(header.get_length());
    response_wire_->read(reinterpret_cast<signed char*>(r_msg.data()));
    //
    parse_response_result result;
    parse_response_header(r_msg, result);
    if (do_check) {
        EXPECT_EQ(type, result.payload_type_);
    }
    // ASSERT_TRUE(parse_response_header(r_msg, result));
    // EXPECT_EQ(session_id_, result.session_id_);
    message = result.payload_;
}

resultset_wires_container* ipc_client::create_resultset_wires() {
    return swc_->create_resultset_wire();
}

void ipc_client::dispose_resultset_wires(resultset_wires_container *rwc) {
    swc_->dispose_resultset_wire(rwc);
}

void ipc_client::handshake() {
    tateyama::proto::endpoint::request::WireInformation wire_information{};
    wire_information.mutable_ipc_information();
    endpoint_handshake_.set_allocated_wire_information(&wire_information);
    tateyama::proto::endpoint::request::Request endpoint_request{};
    endpoint_request.set_allocated_handshake(&endpoint_handshake_);
    send(tateyama::framework::service_id_endpoint_broker, endpoint_request.SerializeAsString());
    endpoint_request.release_handshake();
    endpoint_handshake_.release_wire_information();

    std::string res{};
    receive(res);
}

} // namespace tateyama::endpoint::ipc
