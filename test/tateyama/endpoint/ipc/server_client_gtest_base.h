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
#include <gtest/gtest.h>
#include "server_client_base.h"

namespace tateyama::endpoint::ipc {

class server_client_gtest_base: public server_client_base {
public:
    server_client_gtest_base(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc = 1,
            int nthread = 0) :
            server_client_base(cfg, nproc, nthread) {
    }

    virtual std::shared_ptr<tateyama::framework::service> create_server_service() override = 0;
    virtual void client_thread() override = 0;

protected:
    virtual void check_client_exitcode(int code) override {
        EXPECT_EQ(0, code);
    }

    virtual void client_exit() override {
        std::exit(testing::Test::HasFailure() ? 1 : 0);
    }

};

} // namespace tateyama::endpoint::ipc
