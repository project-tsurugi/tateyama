/*
 * Copyright 2025-2025 Project Tsurugi.
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
#include <cstdlib>

#include <tateyama/framework/server.h>
#include <tateyama/framework/component_ids.h>

#include <tateyama/proto/system/request.pb.h>
#include <tateyama/proto/system/response.pb.h>
#include <tateyama/system/service/bridge.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::system {

static constexpr std::size_t SERVICE_MESSAGE_VERSION_MAJOR = 0;
static constexpr std::size_t SERVICE_MESSAGE_VERSION_MINOR = 0;

class system_info_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();
        setenv("TSURUGI_HOME", path().c_str(), 1); // overwrite env

        proto::system::request::Request request{};
        request.set_service_message_version_major(SERVICE_MESSAGE_VERSION_MAJOR);
        request.set_service_message_version_minor(SERVICE_MESSAGE_VERSION_MINOR);
        (void) request.mutable_getsysteminfo();
        auto str = request.SerializeAsString();
        service_request_ = std::make_shared<test_utils::test_request>(10, system::service::system_service_bridge::tag, str);
        service_response_ = std::make_shared<test_utils::test_response>();
    }

    void Start() {
        auto cfg = api::configuration::create_configuration("", test_utils::default_configuration_for_tests);
        set_dbpath(*cfg);
        sv_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        system_service_bridge_ = std::make_shared<system::service::system_service_bridge>();
        sv_->add_service(system_service_bridge_);
        ASSERT_TRUE(sv_->setup());
        ASSERT_TRUE(sv_->start());
    }
    void TearDown() override {
        ASSERT_TRUE(sv_->shutdown());
    }

protected:
    std::shared_ptr<system::service::system_service_bridge> system_service_bridge_{};
    std::shared_ptr<test_utils::test_request> service_request_{};
    std::shared_ptr<test_utils::test_response> service_response_{};

    void set_up_info() {
        auto dir = std::filesystem::path(path()) / std::filesystem::path("lib");
        std::filesystem::create_directory(dir);

        std::ofstream strm(dir / std::filesystem::path("tsurugi-info.json"));
        if (!strm) {
            FAIL();
        }
        strm << R"({
    "name": "tsurugidb",
    "version": "1.1.1-SNAPSHOT",
    "date": "2020-01-01T00:00Z",
    "url": "//tree/0123456789abcdef0123456789abcdef01234567"
}
)";
        strm.close();
    }

private:
    std::unique_ptr<framework::server> sv_{};
};

TEST_F(system_info_test, success) {
    set_up_info();
    Start();
    (*system_service_bridge_)(service_request_, service_response_);

    auto& payload = service_response_->wait_and_get_body();
    ASSERT_FALSE(service_response_->is_error());

    proto::system::response::GetSystemInfo response{};
    ASSERT_TRUE(response.ParseFromString(payload));
    ASSERT_EQ(response.result_case(), proto::system::response::GetSystemInfo::kSuccess);
    auto system_info = response.success().system_info();
    EXPECT_EQ(system_info.name(), "tsurugidb");
    EXPECT_EQ(system_info.version(), "1.1.1-SNAPSHOT");
    EXPECT_EQ(system_info.date(), "2020-01-01T00:00Z");
    EXPECT_EQ(system_info.url(), "//tree/0123456789abcdef0123456789abcdef01234567");
}

TEST_F(system_info_test, not_found) {
    Start();
    (*system_service_bridge_)(service_request_, service_response_);

    auto& payload = service_response_->wait_and_get_body();
    ASSERT_FALSE(service_response_->is_error());

    proto::system::response::GetSystemInfo response{};
    ASSERT_TRUE(response.ParseFromString(payload));
    ASSERT_EQ(response.result_case(), proto::system::response::GetSystemInfo::kError);
    EXPECT_EQ(response.error().code(), proto::system::diagnostic::ErrorCode::NOT_FOUND);
}

TEST_F(system_info_test, message_version_major) {
    proto::system::request::Request request{};
    request.set_service_message_version_major(SERVICE_MESSAGE_VERSION_MAJOR + 1);
    request.set_service_message_version_minor(SERVICE_MESSAGE_VERSION_MINOR);
    (void) request.mutable_getsysteminfo();
    auto service_request = std::make_shared<test_utils::test_request>(10, system::service::system_service_bridge::tag, request.SerializeAsString());

    Start();
    (*system_service_bridge_)(service_request, service_response_);
    ASSERT_TRUE(service_response_->wait_and_get_body().empty());
    ASSERT_TRUE(service_response_->is_error());

    auto& payload = service_response_->get_error();
    proto::diagnostics::Record record{};
    ASSERT_TRUE(record.ParseFromString(payload));
    ASSERT_EQ(record.code(), proto::diagnostics::Code::INVALID_REQUEST);
}

TEST_F(system_info_test, message_version_minor) {
    proto::system::request::Request request{};
    request.set_service_message_version_major(SERVICE_MESSAGE_VERSION_MAJOR);
    request.set_service_message_version_minor(SERVICE_MESSAGE_VERSION_MINOR + 1);
    (void) request.mutable_getsysteminfo();
    auto service_request = std::make_shared<test_utils::test_request>(10, system::service::system_service_bridge::tag, request.SerializeAsString());

    Start();
    (*system_service_bridge_)(service_request, service_response_);
    ASSERT_TRUE(service_response_->wait_and_get_body().empty());
    ASSERT_TRUE(service_response_->is_error());

    auto& payload = service_response_->get_error();
    proto::diagnostics::Record record{};
    ASSERT_TRUE(record.ParseFromString(payload));
    ASSERT_EQ(record.code(), proto::diagnostics::Code::INVALID_REQUEST);
}

} // namespace tateyama::system
