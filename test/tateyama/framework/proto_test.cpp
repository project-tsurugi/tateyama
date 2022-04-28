/*
 * Copyright 2018-2020 tsurugi project.
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
#include <tateyama/framework/router.h>
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
    proto::framework::response::Header hdr{};

}

}
