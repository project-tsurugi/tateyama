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

#include "authentication_adapter_test.h"

#include <gtest/gtest.h>

namespace tateyama::authentication::resource {

class user_name_test : public ::testing::Test {

public:
    std::optional<std::string> test_target_{};
};

TEST_F(user_name_test, basic) {
    test_target_ = "abcdefgh";
    auto result = authentication_adapter_test::check(test_target_);

    EXPECT_EQ(test_target_, result);
}

TEST_F(user_name_test, japanese) {
    test_target_ = u8"あいうえお";
    auto result = authentication_adapter_test::check(test_target_);

    EXPECT_EQ(test_target_, result);
}

TEST_F(user_name_test, length_limit) {
    test_target_ = std::string(1024, 'u');
    auto result = authentication_adapter_test::check(test_target_);

    EXPECT_EQ(test_target_, result);
}

TEST_F(user_name_test, begin_space) {
    test_target_ = " abcdefgh";

    EXPECT_THROW(authentication_adapter_test::check(test_target_), tateyama::authentication::resource::authentication_exception);
}

TEST_F(user_name_test, end_space) {
    test_target_ = "abcdefgh ";

    EXPECT_THROW(authentication_adapter_test::check(test_target_), tateyama::authentication::resource::authentication_exception);
}

TEST_F(user_name_test, control) {
    test_target_ = "abcd\nefgh ";

    EXPECT_THROW(authentication_adapter_test::check(test_target_), tateyama::authentication::resource::authentication_exception);
}

TEST_F(user_name_test, length_over) {
    test_target_ = std::string(1025, 'u');

    EXPECT_THROW(authentication_adapter_test::check(test_target_), tateyama::authentication::resource::authentication_exception);
}

} // namespace tateyama::endpoint::ipc
