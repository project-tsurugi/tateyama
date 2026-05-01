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

#include <tateyama/api/configuration.h>
#include <tateyama/framework/environment.h>

#include <tateyama/configuration/configuration_provider.h>
#include "tateyama/configuration//resource/database_info_impl.h"
#include "utility.h"

#include <gtest/gtest.h>

namespace tateyama::test_utils {

class configuration_provider : public tateyama::configuration::configuration_provider {
public:
    configuration_provider() : database_info_(&database_info_for_test) {
    }
    [[nodiscard]] api::server::database_info const& database_info() const noexcept override {
        return *database_info_;
    }
    bool setup(framework::environment&) override {
        return true;
    }
    bool start(framework::environment&) override {
        return true;
    }
    bool shutdown(framework::environment&) override {
        return true;
    }
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    [[nodiscard]] std::string_view label() const noexcept override {
        return "configuration_for_test";
    }

    void set_database_info(tateyama::api::server::database_info* database_info) {
        database_info_ = database_info;
    }

private:
    tateyama::configuration::resource::database_info_impl database_info_for_test{"test_database", "test_instance"};
    tateyama::api::server::database_info* database_info_;
};

class Test : public ::testing::Test {
public:
    Test() {
        configuration_provider_ = std::make_shared<configuration_provider>();
        test_environment_.resource_repository().add(configuration_provider_);
    }
    void set_database_info(tateyama::api::server::database_info* database_info) {
        configuration_provider_->set_database_info(database_info);
    }

protected:
    std::istringstream iss_{""};
    std::shared_ptr<configuration_provider> configuration_provider_{};
    std::shared_ptr<tateyama::api::configuration::whole> test_configuration_{std::make_shared<tateyama::api::configuration::whole>(iss_, default_configuration_for_tests)};
    tateyama::framework::environment test_environment_{framework::boot_mode::database_server, test_configuration_};
};

}
