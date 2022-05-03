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
#include <tateyama/server/impl/response.h>

#include <tateyama/api/endpoint/response.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/test.pb.h>

#include <gtest/gtest.h>

namespace tateyama::api {

using namespace std::literals::string_literals;

class server_response_test : public ::testing::Test {

};

using namespace std::string_view_literals;

class test_response : public endpoint::response {
public:
    test_response() = default;

    explicit test_response(std::string_view data) :
        data_(data)
    {}

    void code(endpoint::response_code code) override {

    }
    status body(std::string_view body) override {
        data_ = body;
        return status::ok;
    }

    status body_head(std::string_view body_head) override {
        return status::ok;
    }

    status acquire_channel(std::string_view name, endpoint::data_channel*& ch) override {
        return status::ok;
    }

    status release_channel(endpoint::data_channel& ch) override {
        return status::ok;
    }

    status close_session() override {
        return status::ok;
    }
    std::string data_{};  //NOLINT
};

TEST_F(server_response_test, session_id_is_added_as_header) {
    using namespace std::string_view_literals;
    std::stringstream ss{};
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(999);
        msg.set_message_version(1);
        ASSERT_TRUE(msg.SerializeToOstream(&ss));
    }

    auto str = ss.str();
    auto epres = std::make_shared<test_response>();

    api::server::impl::response svrres{epres};
    svrres.session_id(100);
    ASSERT_EQ(status::ok, svrres.body(str));

    auto result = epres->data_;
    ::tateyama::proto::framework::response::Header out100{}, etc{};
    bool clean_eof{false};
    google::protobuf::io::ArrayInputStream in{result.data(), static_cast<int>(result.size())};
    EXPECT_TRUE(utils::ParseDelimitedFromZeroCopyStream(&out100, &in, &clean_eof));
    EXPECT_FALSE(clean_eof);
    std::string_view out{};
    EXPECT_TRUE(utils::GetDelimitedBodyFromZeroCopyStream(&in, &clean_eof, out));
    EXPECT_FALSE(clean_eof);

    EXPECT_FALSE(utils::ParseDelimitedFromZeroCopyStream(&etc, &in, &clean_eof));
    EXPECT_TRUE(clean_eof);
    ::tateyama::proto::test::TestMsg msg{};
    EXPECT_TRUE(msg.ParseFromArray(out.data(), out.size()));

    EXPECT_EQ(100, out100.session_id());
    EXPECT_EQ(1, msg.message_version());
    EXPECT_EQ(999, msg.id());

}

}
