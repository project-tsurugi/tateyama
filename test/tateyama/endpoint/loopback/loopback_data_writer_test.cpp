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
#include <tateyama/endpoint/loopback/loopback_data_writer.h>

#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class loopback_data_writer_test: public loopback_test_base {
};

TEST_F(loopback_data_writer_test, printable) {
    std::vector<std::string> test_data = {"hello", "this is a pen"};
    tateyama::common::loopback::loopback_data_writer writer {};
    {
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 0);
    }
    {
        const std::string &data = test_data[0];
        EXPECT_EQ(writer.write(data.data(), data.length()), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 0);
    }
    {
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        const std::string &data = test_data[1];
        EXPECT_EQ(writer.write(data.data(), data.length()), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 2);
        EXPECT_EQ(commit[0], test_data[0]);
        EXPECT_EQ(commit[1], test_data[1]);
    }
    {
        // re-get data
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 2);
        EXPECT_EQ(commit[0], test_data[0]);
        EXPECT_EQ(commit[1], test_data[1]);
    }
}

TEST_F(loopback_data_writer_test, unnesessary_call) {
    std::vector<std::string> test_data = {"hello", "this is a pen"};
    tateyama::common::loopback::loopback_data_writer writer {};
    {
        // commit() without write()
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 0);
    }
    {
        EXPECT_EQ(writer.write(test_data[0].data(), test_data[0].length()), tateyama::status::ok);
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        // commit() without write()
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        // empty write
        EXPECT_EQ(writer.write("", 0), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        // commit() after empty write()
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        // data is not empty, but length is specified as 0
        EXPECT_EQ(writer.write("hello", 0), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
    {
        // commit() after empty write()
        EXPECT_EQ(writer.commit(), tateyama::status::ok);
        const auto commit = writer.committed_data();
        EXPECT_EQ(commit.size(), 1);
        EXPECT_EQ(commit[0], test_data[0]);
    }
}

TEST_F(loopback_data_writer_test, drop_commit) {
    std::vector<std::string> test_data = {"hello", "this is a pen"};
    tateyama::common::loopback::loopback_data_writer writer {};

    EXPECT_EQ(writer.write(test_data[0].data(), test_data[0].length()), tateyama::status::ok);
    EXPECT_EQ(writer.write(test_data[1].data(), test_data[1].length()), tateyama::status::ok);
    EXPECT_EQ(writer.commit(), tateyama::status::ok);
    const auto commit = writer.committed_data();
    EXPECT_EQ(commit.size(), 1);
    EXPECT_EQ(commit[0], std::string {test_data[0] + test_data[1]});
}

TEST_F(loopback_data_writer_test, binary) {
    const int len = 256;
    char data[len];
    for (int i = 0; i < len; i++) {
        data[i] = static_cast<char>(i);
    }
    tateyama::common::loopback::loopback_data_writer writer {};
    writer.write(data, len);
    writer.commit();
    const auto commit = writer.committed_data();
    EXPECT_EQ(commit.size(), 1);
    const std::string &commitstr = commit[0];
    EXPECT_EQ(commitstr.length(), len);
    const char *commitptr = commitstr.c_str();
    for (int i = 0; i < len; i++) {
        EXPECT_EQ(commitstr[i], static_cast<char>(i));
        EXPECT_EQ(commitptr[i], static_cast<char>(i));
    }
}

}  // namespace tateyama::loopback
