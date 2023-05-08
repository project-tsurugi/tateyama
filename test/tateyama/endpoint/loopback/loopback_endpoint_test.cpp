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
#include <tateyama/endpoint/loopback/loopback_endpoint.h>

#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class loopback_endpoint_test: public loopback_test_base {
};

/*
 * see tateyama/test/loopback/loopback_client_test.cpp for real test.
 * this test is created just for 100% coverage.
 */
TEST_F(loopback_endpoint_test, label) {
    tateyama::endpoint::loopback::loopback_endpoint endpoint { };
    std::string label { endpoint.label() };
    EXPECT_GT(label.length(), 0);
    EXPECT_NE(label.find("loopback"), std::string::npos);
    EXPECT_NE(label.find("endpoint"), std::string::npos);
}

} // namespace tateyama::api::endpoint::loopback
