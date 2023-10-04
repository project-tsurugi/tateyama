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

#pragma once
#include "ipc_test_env.h"
#include "ipc_client.h"
#include "server_client_gtest_base.h"

namespace tateyama::api::endpoint::ipc {

class ipc_gtest_base: public ::testing::Test, public ipc_test_env {
    void SetUp() override {
        ipc_test_env::setup();
    }

    void TearDown() override {
        ipc_test_env::teardown();
    }
};

} // namespace tateyama::api::endpoint::ipc
