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
#include <tateyama/framework/repository.h>

#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/environment.h>

#include <gtest/gtest.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;

class proto_test : public ::testing::Test {

};

using namespace std::string_view_literals;

TEST_F(proto_test, basic) {
    proto::framework::request::Header reqhdr{};
    reqhdr.set_message_version(1);
    reqhdr.set_service_id(10);
    reqhdr.set_session_id(100);
    EXPECT_EQ(1, reqhdr.message_version());
    EXPECT_EQ(10, reqhdr.service_id());
    EXPECT_EQ(100, reqhdr.session_id());

    proto::framework::response::Header reshdr{};
    reshdr.set_session_id(99);
    EXPECT_EQ(99, reshdr.session_id());
}

}
