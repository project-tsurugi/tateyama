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
#include <tateyama/loopback/buffered_response.h>

#include <gtest/gtest.h>

namespace tateyama::loopback {

class buffered_response_test: public ::testing::Test {
public:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(buffered_response_test, empty) {
    buffered_response response{};

    EXPECT_EQ(response.session_id(), 0);
    EXPECT_EQ(response.code(), tateyama::api::server::response_code::success);
    EXPECT_EQ(response.body_head().length(), 0);
    EXPECT_EQ(response.body().length(), 0);
    EXPECT_FALSE(response.has_channel(""));
    EXPECT_FALSE(response.has_channel("name"));
    try {
        const auto &data = response.channel("name");
        FAIL();
        EXPECT_EQ(data.size(), 0);
    } catch (std::invalid_argument &ex) {
        std::cout << ex.what() << std::endl;
        SUCCEED();
    }
}

TEST_F(buffered_response_test, multi_channel) {
    const std::size_t session_id = 123;
    const tateyama::api::server::response_code code = tateyama::api::server::response_code::success;
    const std::string body_head { "body_head" };
    const std::string body { "body message" };
    const std::vector<std::string> names = { "channelA", "channelB" };
    std::map<std::string, std::vector<std::string>> test_data { };
    test_data[names[0]] = { "hello", "this is a pen" };
    test_data[names[1]] = { "good night", "it's fine today" };
    {
        buffered_response response{session_id, code, body_head, body, test_data};
        EXPECT_EQ(response.session_id(), session_id);
        EXPECT_EQ(response.code(), code);
        EXPECT_EQ(response.body_head(), body_head);
        EXPECT_EQ(response.body(), body);
        EXPECT_FALSE(response.has_channel(""));
        EXPECT_FALSE(response.has_channel("name"));
        for (const auto &[name, data] : test_data) {
            EXPECT_TRUE(response.has_channel(name));
            const auto &committed = response.channel(name);
            ASSERT_EQ(data.size(), committed.size());
            for (int i = 0; i < data.size(); i++) {
                EXPECT_EQ(data[i], committed[i]);
            }
        }
    }
    {
        // empty body_head
        buffered_response response{session_id, code, "", body, test_data};
        EXPECT_EQ(response.session_id(), session_id);
        EXPECT_EQ(response.code(), code);
        EXPECT_EQ(response.body_head().length(), 0);
        EXPECT_EQ(response.body(), body);
    }
    {
        // empty body
        buffered_response response{session_id, code, body_head, "", test_data};
        EXPECT_EQ(response.session_id(), session_id);
        EXPECT_EQ(response.code(), code);
        EXPECT_EQ(response.body_head(), body_head);
        EXPECT_EQ(response.body().length(), 0);
    }
    {
        // empty body head and body
        buffered_response response{session_id, code, "", "", test_data};
        EXPECT_EQ(response.session_id(), session_id);
        EXPECT_EQ(response.code(), code);
        EXPECT_EQ(response.body_head().length(), 0);
        EXPECT_EQ(response.body().length(), 0);
    }
}

} // namespace tateyama::loopback