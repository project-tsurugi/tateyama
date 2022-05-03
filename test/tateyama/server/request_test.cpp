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
#include <tateyama/api/server/request.h>

#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/server/request.h>
#include <tateyama/server/impl/request.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/test.pb.h>
#include <tateyama/utils/protobuf_utils.h>

#include <gtest/gtest.h>

namespace tateyama::api {

using namespace std::literals::string_literals;

class server_request_test : public ::testing::Test {

};

using namespace std::string_view_literals;

class test_request : public endpoint::request {
public:
    test_request() = default;

    explicit test_request(std::string_view data) :
        data_(data)
    {}

    [[nodiscard]] std::string_view payload() const override {
        return data_;
    }

    std::string data_{};  //NOLINT
};

TEST_F(server_request_test, basic) {

    using namespace std::string_view_literals;
    std::stringstream ss{};
    {
        ::tateyama::proto::framework::request::Header hdr100{};
        hdr100.set_service_id(100);
        hdr100.set_session_id(101);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(hdr100, &ss));
    }
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(999);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(msg, &ss));
    }

    auto str = ss.str();
    auto epreq = std::make_shared<test_request>(str);

    auto svrreq = server::impl::create_request(epreq);

    ASSERT_TRUE(svrreq);
    EXPECT_EQ(100, svrreq->service_id());
    EXPECT_EQ(101, svrreq->session_id());
    auto pl = svrreq->payload();
    ::tateyama::proto::test::TestMsg out{};
    ASSERT_TRUE(out.ParseFromArray(pl.data(), pl.size()));
    EXPECT_EQ(999, out.id());
}

}
