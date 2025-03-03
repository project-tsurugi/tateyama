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
#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/test_utils/utility.h>

#include <tateyama/loopback/loopback_client.h>

namespace tateyama::api::endpoint::loopback {

class loopback_test_base: public ::testing::Test, public test_utils::utility {
    void SetUp() override {
        temporary_.prepare();
        cfg_ = tateyama::api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
        set_dbpath(*cfg_);
    }
    void TearDown() override {
        temporary_.clean();
    }

protected:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_ { };
};

} //namespace tateyama::api::endpoint::loopback
