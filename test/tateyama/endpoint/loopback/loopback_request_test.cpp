/*
 * Copyright 2019-2023 tsurugi project.
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
#include <tateyama/endpoint/loopback/loopback_request.h>

#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class loopback_request_test: public loopback_test_base {
};

TEST_F(loopback_request_test, simple) {
    const std::size_t session_id = 123;
    const std::size_t service_id = 65478;
    const std::string payload {"hello"};
    tateyama::common::loopback::loopback_request request {session_id, service_id, payload};
    EXPECT_EQ(request.session_id(), session_id);
    EXPECT_EQ(request.service_id(), service_id);
    EXPECT_EQ(request.payload(), payload);
}

TEST_F(loopback_request_test, empty_payload) {
    const std::size_t session_id = 123;
    const std::size_t service_id = 65478;
    const std::string payload {""};
    tateyama::common::loopback::loopback_request request {session_id, service_id, payload};
    EXPECT_EQ(request.session_id(), session_id);
    EXPECT_EQ(request.service_id(), service_id);
    EXPECT_EQ(request.payload(), payload);
}

TEST_F(loopback_request_test, big_binary_payload) {
    const std::size_t session_id = 123;
    const std::size_t service_id = 65478;
    const std::size_t req_len = 4 * 1024;
    std::string payload {};
    payload.resize(req_len);
    for (auto i = 0; i < req_len; i++) {
        payload[i] = static_cast<char>(i % 256);
    }
    tateyama::common::loopback::loopback_request request {session_id, service_id, payload};
    EXPECT_EQ(request.session_id(), session_id);
    EXPECT_EQ(request.service_id(), service_id);
    EXPECT_EQ(request.payload(), payload);
}

}
 // namespace tateyama::api::endpoint::loopback
