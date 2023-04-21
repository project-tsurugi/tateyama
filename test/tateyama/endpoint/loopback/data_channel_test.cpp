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
#include <tateyama/endpoint/loopback/loopback_data_channel.h>

#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class data_channel_test: public loopback_test_base {
};

TEST_F(data_channel_test, name) {
    const std::string name {"channelX"};
    tateyama::common::loopback::loopback_data_channel channel(name);
    EXPECT_EQ(channel.name(), name);
}

TEST_F(data_channel_test, no_name) {
    try {
        tateyama::common::loopback::loopback_data_channel channel("");
        FAIL();
    } catch (std::invalid_argument &ex) {
        std::cout << ex.what() << std::endl;
        SUCCEED();
    }
}

TEST_F(data_channel_test, single_channel) {
    const std::string name {"channelX"};
    tateyama::common::loopback::loopback_data_channel channel(name);

    std::shared_ptr<tateyama::api::server::writer> wrt;
    EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
    tateyama::common::loopback::loopback_data_writer *writer = dynamic_cast<tateyama::common::loopback::loopback_data_writer *>(wrt.get());
    {
        std::vector<std::string> whole {};
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 0);
    }
    std::vector<std::string> test_data {"hello", "this is a pen"};
    {
        const std::string &data = test_data[0];
        EXPECT_EQ(writer->write(data.data(), data.length()), tateyama::status::ok);
        std::vector<std::string> whole {};
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 0);
        EXPECT_EQ(channel.name(), name);
    }
    {
        EXPECT_EQ(writer->commit(), tateyama::status::ok);
        std::vector<std::string> whole {};
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 1);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(channel.name(), name);
    }
    {
        const std::string &data = test_data[1];
        EXPECT_EQ(writer->write(data.data(), data.length()), tateyama::status::ok);
        std::vector<std::string> whole {};
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 1);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(channel.name(), name);
    }
    {
        EXPECT_EQ(writer->commit(), tateyama::status::ok);
        std::vector<std::string> whole {};
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 2);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(whole[1], test_data[1]);
        EXPECT_EQ(channel.name(), name);
    }
    {
        std::vector<std::string> whole {};
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 2);
        whole.clear();
        EXPECT_EQ(whole.size(), 0);
        // still got same data
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 2);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(whole[1], test_data[1]);
        EXPECT_EQ(channel.name(), name);
    }
}

}
 // namespace tateyama::api::endpoint::loopback
