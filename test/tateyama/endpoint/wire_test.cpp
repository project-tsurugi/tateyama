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

#include "server_wires_impl.h"

#include <gtest/gtest.h>

namespace tateyama::api::endpoint::ipc {

class wire_test : public ::testing::Test {
    virtual void SetUp() {
        rv_ = system("if [ -f /dev/shm/tateyama-test ]; then rm -f /dev/shm/tateyama-test; fi ");
        wire_ = std::make_unique<tateyama::common::wire::server_wire_container_impl>("tateyama-test");
    }
    virtual void TearDown() {
        rv_ = system("if [ -f /dev/shm/tateyama-test ]; then rm -f /dev/shm/tateyama-test*; fi ");
    }

    int rv_;

public:
    std::string request_test_message_{};
    std::unique_ptr<tateyama::common::wire::server_wire_container_impl> wire_;
};

TEST_F(wire_test, loop) {
    auto* request_wire = static_cast<tateyama::common::wire::server_wire_container_impl::wire_container_impl*>(wire_->get_request_wire());

    request_test_message_.resize(3827);
    char *p = request_test_message_.data();
    for (std::size_t i = 0; i < request_test_message_.length(); i++) {
        *(p++) = (i * 7)  % 255;
    }
    for (std::size_t n = 0; n < 1000; n++) {
        request_wire->brand_new();
        const char *ptr = request_test_message_.data();
        for (std::size_t i = 0; i < request_test_message_.length(); ptr++, i++) {
            request_wire->write(*ptr);
        }
        tateyama::common::wire::message_header::index_type index = n % 11;
        request_wire->flush(tateyama::common::wire::message_header(index, request_test_message_.length()));

        auto h = request_wire->peep(true);
        EXPECT_EQ(index, h.get_idx());
        auto length = h.get_length();
        EXPECT_EQ(request_test_message_.length(), length);
        auto read_point = request_wire->read_point();
        
        std::string message;
        message.resize(length);
        memcpy(message.data(), request_wire->payload(length), length);
        EXPECT_EQ(memcmp(request_test_message_.data(), message.data(), length), 0);

        request_wire->dispose(read_point);
    }
}

}  // namespace tateyama::api::endpoint::ipc
