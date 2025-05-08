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
#include <thread>

#include <tateyama/status.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include <tateyama/endpoint/ipc/bootstrap/server_wires_impl.h>

#include <gtest/gtest.h>

namespace tateyama::endpoint::ipc {

class response_wire_test : public ::testing::Test {
    static constexpr std::size_t datachannel_buffer_size = 64 * 1024;

    void SetUp() override {
        rv_ = system("if [ -f /dev/shm/tateyama-response_wire_test ]; then rm -f /dev/shm/tateyama-response_wire_test; fi ");
        wire_ = std::make_unique<bootstrap::server_wire_container_impl>("tateyama-response_wire_test", "dummy_mutex_file_name", datachannel_buffer_size, 16);
    }
    void TearDown() override {
        rv_ = system("if [ -f /dev/shm/tateyama-response_wire_test ]; then rm -f /dev/shm/tateyama-response_wire_test*; fi ");
    }

    int rv_;

public:
    std::string response_test_message_{};
    std::unique_ptr<bootstrap::server_wire_container_impl> wire_;
};

TEST_F(response_wire_test, large_messege) {
    static constexpr std::size_t string_length = 132397;

    auto& response_wire = dynamic_cast<bootstrap::server_wire_container_impl::response_wire_container_impl&>(wire_->get_response_wire());

    response_test_message_.resize(string_length);
    char *p = response_test_message_.data();
    for (std::size_t i = 0; i < response_test_message_.length(); i++) {
        *(p++) = (i * 7)  % 255;
    }
    tateyama::common::wire::message_header::index_type index = 11;

    std::thread th([this, &response_wire, index]{
                       response_wire.write(response_test_message_.data(), tateyama::common::wire::response_header(index, response_test_message_.length(), 1));
                   });

    response_wire.await();
    EXPECT_EQ(index, response_wire.get_idx());
    auto length = response_wire.get_length();
    EXPECT_EQ(response_test_message_.length(), length);

    std::string recv_message;
    recv_message.resize(length);
    response_wire.read(recv_message.data());
    EXPECT_EQ(memcmp(response_test_message_.data(), recv_message.data(), length), 0);

    th.join();
}

}  // namespace tateyama::api::endpoint::ipc
