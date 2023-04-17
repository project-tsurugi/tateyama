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
#pragma once

#include <gtest/gtest.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/utils/test_utils.h>

#include <tateyama/endpoint/loopback/bootstrap/loopback_endpoint.h>

namespace tateyama::api::endpoint::loopback {

class loopback_test_base: public ::testing::Test, public test::test_utils {
    void SetUp() override {
        temporary_.prepare();
        cfg_ = tateyama::api::configuration::create_configuration("");
        set_dbpath(*cfg_);
    }
    void TearDown() override {
        temporary_.clean();
    }

protected:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_ { };
};

} //namespace tateyama::api::endpoint::loopback
