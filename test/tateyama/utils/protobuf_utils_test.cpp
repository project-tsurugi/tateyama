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
#include <tateyama/utils/protobuf_utils.h>

#include <gtest/gtest.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <tateyama/proto/framework/request.pb.h>

namespace tateyama::api {

using namespace std::literals::string_literals;

class protobuf_utils_test : public ::testing::Test {};

using namespace std::string_view_literals;

TEST_F(protobuf_utils_test, delimited_stream) {
    using namespace std::string_view_literals;
    std::stringstream ss{};
    {
        ::tateyama::proto::framework::request::Header hdr100{};
        hdr100.set_service_id(100);
        hdr100.set_session_id(101);
        if (!utils::SerializeDelimitedToOstream(hdr100, &ss)) {
            FAIL();
        }
    }
    {
        ::tateyama::proto::framework::request::Header hdr200{};
        hdr200.set_service_id(200);
        hdr200.set_session_id(201);
        if (!utils::SerializeDelimitedToOstream(hdr200, &ss)) {
            FAIL();
        }
    }
    {
        ::tateyama::proto::framework::request::Header hdr300{};
        hdr300.set_service_id(300);
        hdr300.set_session_id(301);
        if (!utils::SerializeDelimitedToOstream(hdr300, &ss)) {
            FAIL();
        }
    }
    ::tateyama::proto::framework::request::Header out100{}, out200{}, out300{}, etc{};
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
    EXPECT_EQ(100, out100.service_id());
    EXPECT_EQ(101, out100.session_id());
    EXPECT_EQ(200, out200.service_id());
    EXPECT_EQ(201, out200.session_id());
    EXPECT_EQ(300, out300.service_id());
    EXPECT_EQ(301, out300.session_id());

}

TEST_F(protobuf_utils_test, get_delimited_body) {
    using namespace std::string_view_literals;
    std::stringstream ss{};
    {
        ::tateyama::proto::framework::request::Header hdr100{};
        hdr100.set_service_id(100);
        hdr100.set_session_id(101);
        if (!utils::SerializeDelimitedToOstream(hdr100, &ss)) {
            FAIL();
        }
    }
    {
        ::tateyama::proto::framework::request::Header hdr200{};
        hdr200.set_service_id(200);
        hdr200.set_session_id(201);
        if (!utils::SerializeDelimitedToOstream(hdr200, &ss)) {
            FAIL();
        }
    }
    ::tateyama::proto::framework::request::Header out100{}, out200{}, etc{};
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

    EXPECT_EQ(100, out100.service_id());
    EXPECT_EQ(101, out100.session_id());
    EXPECT_EQ(200, out200.service_id());
    EXPECT_EQ(201, out200.session_id());

}
}
