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

#include <cstdio>
#include <memory>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <stdlib.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/component_ids.h>

#include <tateyama/proto/system/request.pb.h>
#include <tateyama/proto/system/response.pb.h>
#include "tateyama/endpoint/ipc/ipc_client.h"

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::system {

static constexpr std::size_t SERVICE_MESSAGE_VERSION_MAJOR = 0;
static constexpr std::size_t SERVICE_MESSAGE_VERSION_MINOR = 1;

class version_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();
        setenv("TSURUGI_HOME", path().c_str(), 1); // overwrite env

        request_.set_service_message_version_major(SERVICE_MESSAGE_VERSION_MAJOR);
        request_.set_service_message_version_minor(SERVICE_MESSAGE_VERSION_MINOR);
        (void) request_.mutable_getdatabaseproductversion();
    }

    void Start() {
        auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
        set_dbpath(*cfg);
        sv_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        tateyama::test_utils::add_core_components_for_test(*sv_);
        ASSERT_TRUE(sv_->setup());
        ASSERT_TRUE(sv_->start());
        
        client_ = std::make_unique<endpoint::ipc::ipc_client>(cfg);
    }
    void TearDown() override {
        ASSERT_TRUE(sv_->shutdown());
    }

protected:
    std::unique_ptr<endpoint::ipc::ipc_client> client_{};
    proto::system::request::Request request_{};

    void set_up_info() {
        auto dir = std::filesystem::path(path()) / std::filesystem::path("lib");
        std::filesystem::create_directory(dir);

        std::ofstream strm(dir / std::filesystem::path("tsurugi-info.json"));
        if (!strm) {
            FAIL();
        }
        strm << R"({"name": "tsurugidb",
"version": "1.1.1-SNAPSHOT",
"date": "2020-01-23T45:21Z",
"url": "//tree/0123456789abcdef0123456789abcdef01234567"
})";
        strm.close();
    }

private:
    std::unique_ptr<framework::server> sv_{};
};

TEST_F(version_test, success) {
    set_up_info();
    Start();
    client_->send(framework::service_id_system, request_.SerializeAsString());

    std::string payload{};
    proto::framework::response::Header::PayloadType type{};
    client_->receive(payload, type);
    ASSERT_EQ(type, proto::framework::response::Header::SERVICE_RESULT);

    proto::system::response::GetDatabaseProductVersion response{};
    ASSERT_TRUE(response.ParseFromString(payload));
    ASSERT_EQ(response.result_case(), tateyama::proto::system::response::GetDatabaseProductVersion::kSuccess);
    EXPECT_EQ(response.success().product_version(), "1.1.1-SNAPSHOT");
}

TEST_F(version_test, not_found) {
    Start();
    client_->send(framework::service_id_system, request_.SerializeAsString());

    std::string payload{};
    proto::framework::response::Header::PayloadType type{};
    client_->receive(payload, type);
    ASSERT_EQ(type, proto::framework::response::Header::SERVICE_RESULT);

    proto::system::response::GetDatabaseProductVersion response{};
    ASSERT_TRUE(response.ParseFromString(payload));
    ASSERT_EQ(response.result_case(), proto::system::response::GetDatabaseProductVersion::kError);
    EXPECT_EQ(response.error().code(), proto::system::diagnostic::ErrorCode::NOT_FOUND);
}

} // namespace tateyama::system
