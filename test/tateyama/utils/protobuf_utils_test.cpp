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
#include <tateyama/utils/protobuf_utils.h>

#include <gtest/gtest.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <tateyama/proto/test.pb.h>

namespace tateyama::api {

using namespace std::literals::string_literals;

class protobuf_utils_test : public ::testing::Test {};

using namespace std::string_view_literals;

TEST_F(protobuf_utils_test, delimited_stream) {
    using namespace std::string_view_literals;
    std::stringstream ss{};
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(100);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(msg, &ss));
    }
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(200);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(msg, &ss));
    }
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(300);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(msg, &ss));
    }
    ::tateyama::proto::test::TestMsg out100{}, out200{}, out300{}, etc{};
    bool clean_eof{false};
    google::protobuf::io::IstreamInputStream in{&ss};
    EXPECT_TRUE(utils::ParseDelimitedFromZeroCopyStream(&out100, &in, &clean_eof));
    EXPECT_FALSE(clean_eof);
    EXPECT_TRUE(utils::ParseDelimitedFromZeroCopyStream(&out200, &in, &clean_eof));
    EXPECT_FALSE(clean_eof);
    EXPECT_TRUE(utils::ParseDelimitedFromZeroCopyStream(&out300, &in, &clean_eof));
    EXPECT_FALSE(clean_eof);
    EXPECT_FALSE(utils::ParseDelimitedFromZeroCopyStream(&etc, &in, &clean_eof));
    EXPECT_TRUE(clean_eof);
    EXPECT_EQ(100, out100.id());
    EXPECT_EQ(200, out200.id());
    EXPECT_EQ(300, out300.id());

}

TEST_F(protobuf_utils_test, get_delimited_body) {
    using namespace std::string_view_literals;
    std::stringstream ss{};
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(100);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(msg, &ss));
    }
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(200);
        ASSERT_TRUE(utils::SerializeDelimitedToOstream(msg, &ss));
    }
    ::tateyama::proto::test::TestMsg out100{}, out200{}, etc{};
    bool clean_eof{false};

    std::string str = ss.str();
    google::protobuf::io::ArrayInputStream in{str.data(), static_cast<int>(str.size())};
    EXPECT_TRUE(utils::ParseDelimitedFromZeroCopyStream(&out100, &in, &clean_eof));
    EXPECT_FALSE(clean_eof);
    std::string_view out{};
    EXPECT_TRUE(utils::GetDelimitedBodyFromZeroCopyStream(&in, &clean_eof, out));
    EXPECT_FALSE(clean_eof);

    EXPECT_FALSE(utils::ParseDelimitedFromZeroCopyStream(&etc, &in, &clean_eof));
    EXPECT_TRUE(clean_eof);
    EXPECT_TRUE(out200.ParseFromArray(out.data(), out.size()));

    EXPECT_EQ(100, out100.id());
    EXPECT_EQ(200, out200.id());

}

TEST_F(protobuf_utils_test, put_delimited_body) {
    using namespace std::string_view_literals;
    std::stringstream ss{};
    std::string buf{};
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(100);
        ASSERT_TRUE(msg.SerializeToString(&buf));
        ASSERT_TRUE(utils::PutDelimitedBodyToOstream(buf, &ss));
    }
    {
        ::tateyama::proto::test::TestMsg msg{};
        msg.set_id(200);
        ASSERT_TRUE(msg.SerializeToString(&buf));
        ASSERT_TRUE(utils::PutDelimitedBodyToOstream(buf, &ss));
    }
    ::tateyama::proto::test::TestMsg out100{}, out200{}, etc{};
    bool clean_eof{false};

    std::string str = ss.str();
    google::protobuf::io::ArrayInputStream in{str.data(), static_cast<int>(str.size())};

    EXPECT_TRUE(utils::ParseDelimitedFromZeroCopyStream(&out100, &in, &clean_eof));
    EXPECT_FALSE(clean_eof);
    std::string_view out{};
    EXPECT_TRUE(utils::GetDelimitedBodyFromZeroCopyStream(&in, &clean_eof, out));
    EXPECT_FALSE(clean_eof);

    EXPECT_FALSE(utils::ParseDelimitedFromZeroCopyStream(&etc, &in, &clean_eof));
    EXPECT_TRUE(clean_eof);
    EXPECT_TRUE(out200.ParseFromArray(out.data(), out.size()));

    EXPECT_EQ(100, out100.id());
    EXPECT_EQ(200, out200.id());

}

}
