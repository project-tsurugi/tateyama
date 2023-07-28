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
#include <thread>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include <tateyama/endpoint/ipc/bootstrap/server_wires_impl.h>

#include <gtest/gtest.h>

namespace tateyama::api::endpoint::ipc {

class wire_test : public ::testing::Test {
    static constexpr std::size_t datachannel_buffer_size = 64 * 1024;

    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/tateyama-wire_test ]; then rm -f /dev/shm/tateyama-wire_test; fi ");
        wire_ = std::make_unique<tateyama::common::wire::server_wire_container_impl>("tateyama-wire_test", "dummy_mutex_file_name", datachannel_buffer_size, 16);
    }
    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/tateyama-wire_test ]; then rm -f /dev/shm/tateyama-wire_test*; fi ");
    }

    int rv_;

public:
    std::string request_test_message_{};
    std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire_;
};

TEST_F(wire_test, payload_loop) {
    static constexpr std::size_t string_length = 3827;

    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_test_message_.resize(string_length);
    char *p = request_test_message_.data();
    for (std::size_t i = 0; i < request_test_message_.length(); i++) {
        *(p++) = (i * 7)  % 255;
    }
    for (std::size_t n = 0; n < 1000; n++) {
        tateyama::common::wire::message_header::index_type index = n % 11;
        request_wire->write(request_test_message_.data(), request_test_message_.length(), index);

        auto h = request_wire->peep(true);
        EXPECT_EQ(index, h.get_idx());
        auto length = h.get_length();
        EXPECT_EQ(request_test_message_.length(), length);

        std::string recv_message;
        recv_message.resize(length);
        memcpy(recv_message.data(), request_wire->payload().data(), length);
        EXPECT_EQ(memcmp(request_test_message_.data(), recv_message.data(), length), 0);

        request_wire->dispose();
    }
}

TEST_F(wire_test, read_loop) {
    static constexpr std::size_t string_length = 3827;

    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_test_message_.resize(string_length);
    char *p = request_test_message_.data();
    for (std::size_t i = 0; i < request_test_message_.length(); i++) {
        *(p++) = (i * 7)  % 255;
    }
    for (std::size_t n = 0; n < 1000; n++) {
        tateyama::common::wire::message_header::index_type index = n % 11;
        request_wire->write(request_test_message_.data(), request_test_message_.length(), index);
    
        auto h = request_wire->peep(true);
        EXPECT_EQ(index, h.get_idx());
        auto length = h.get_length();
        EXPECT_EQ(request_test_message_.length(), length);
        
        std::string recv_message;
        recv_message.resize(length);
        request_wire->read(recv_message.data());
        EXPECT_EQ(memcmp(request_test_message_.data(), recv_message.data(), length), 0);

        request_wire->dispose();
    }
}

TEST_F(wire_test, large_messege) {
    static constexpr std::size_t string_length = 131090;

    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_test_message_.resize(string_length);
    char *p = request_test_message_.data();
    for (std::size_t i = 0; i < request_test_message_.length(); i++) {
        *(p++) = (i * 7)  % 255;
    }
    tateyama::common::wire::message_header::index_type index = 11;

    std::thread th([this, request_wire, index]{
                       request_wire->write(request_test_message_.data(), request_test_message_.length(), index);
                   });

    auto h = request_wire->peep(true);
    EXPECT_EQ(index, h.get_idx());
    auto length = h.get_length();
    EXPECT_EQ(request_test_message_.length(), length);

    std::string recv_message;
    recv_message.resize(length);
    request_wire->read(recv_message.data());
    EXPECT_EQ(memcmp(request_test_message_.data(), recv_message.data(), length), 0);

    request_wire->dispose();

    th.join();
}

}  // namespace tateyama::api::endpoint::ipc
