/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include "tateyama/endpoint/common/administrators.h"

#include <gtest/gtest.h>

namespace tateyama::endpoint::common {

class administrators_test: public ::testing::Test {
protected:
    const std::string admin{"admin"};
    const std::string user{"user"};
    const std::string win_user{"win user"};
    const std::string dmy_user{"dummy_user"};
};

TEST_F(administrators_test, normal) {
    administrators names{admin + " , " + user + " ," + win_user};

    EXPECT_TRUE(names.is_administrator(admin));
    EXPECT_TRUE(names.is_administrator(user));
    EXPECT_TRUE(names.is_administrator(win_user));
    EXPECT_FALSE(names.is_administrator(dmy_user));
}

TEST_F(administrators_test, asterisk) {
    administrators names{"*"};

    EXPECT_TRUE(names.is_administrator(admin));
    EXPECT_TRUE(names.is_administrator(user));
    EXPECT_TRUE(names.is_administrator(win_user));
    EXPECT_TRUE(names.is_administrator(dmy_user));
}

} // namespace tateyama::endpoint::ipc
