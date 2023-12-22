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
#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/api/server/request.h>

#include <tateyama/debug/service.h>
#include <tateyama/loopback/loopback_client.h>
#include <tateyama/proto/debug/request.pb.h>
#include <tateyama/proto/debug/response.pb.h>

#include <gtest/gtest.h>

namespace tateyama::debug {

class debug_service_test : public ::testing::Test {
public:
    void SetUp() override {
    }

    void TearDown() override {
    }

    const std::size_t session_id = 123;
    tateyama::framework::component::id_type service_id = tateyama::framework::service_id_debug;

    const static uint64_t default_major = 0;
    const static uint64_t default_minor = 0;

    std::string make_request(tateyama::proto::debug::request::Logging_Level level, std::string &message) {
        std::string s{};
        tateyama::proto::debug::request::Request proto_req{};
        tateyama::proto::debug::request::Logging logging{};
        logging.set_level(level);
        std::string m { message };
        logging.set_allocated_message(&m);
        proto_req.set_allocated_logging(&logging);
        proto_req.set_service_message_version_major(default_major);
        proto_req.set_service_message_version_minor(default_minor);
        EXPECT_TRUE(proto_req.SerializeToString(&s));
        logging.release_message();
        proto_req.release_logging();
        return s;
    }

    void logging_ok(tateyama::loopback::loopback_client &client,
                    tateyama::proto::debug::request::Logging_Level level, std::string &message) {
        std::string s{make_request(level, message)};
        auto buf_res = client.request(session_id, service_id, s);
        auto body = buf_res.body();
        EXPECT_TRUE(body.size() > 0);
        {
            tateyama::proto::debug::response::Void proto_res{};
            EXPECT_TRUE(proto_res.ParseFromArray(body.data(), body.size()));
        }
    }

    void logging_broken_message(tateyama::loopback::loopback_client &client,
                    tateyama::proto::debug::request::Logging_Level level, std::string &message) {
        std::string broken{make_request(level, message) + "X"};
        auto buf_res = client.request(session_id, service_id, broken);
        auto body = buf_res.body();
        EXPECT_TRUE(body.size() > 0);
        {
            tateyama::proto::debug::response::Logging proto_res{};
            EXPECT_TRUE(proto_res.ParseFromArray(body.data(), body.size()));
            const auto &error = proto_res.unknown_error();
            const auto &message = error.message();
            EXPECT_TRUE(message.length() > 0);
            std::cout << message << std::endl;
        }
    }

    void logging_empty_req(tateyama::loopback::loopback_client &client,
                                tateyama::proto::debug::request::Logging_Level level, std::string &message) {
        std::string empty{};
        {
            tateyama::proto::debug::request::Request proto_req{};
            EXPECT_TRUE(proto_req.SerializeToString(&empty));
        }
        auto buf_res = client.request(session_id, service_id, empty);
        auto body = buf_res.body();
        EXPECT_TRUE(body.size() > 0);
        {
            tateyama::proto::debug::response::Logging proto_res{};
            EXPECT_TRUE(proto_res.ParseFromArray(body.data(), body.size()));
            const auto &error = proto_res.unknown_error();
            const auto &message = error.message();
            EXPECT_TRUE(message.length() > 0);
            std::cout << message << std::endl;
        }
    }
};

TEST_F(debug_service_test, simple) {
    auto cfg = api::configuration::create_configuration();
    tateyama::framework::server sv {tateyama::framework::boot_mode::database_server, cfg};
    sv.add_service(std::make_shared<tateyama::framework::routing_service>());
    sv.add_service(std::make_shared<tateyama::debug::service>());
    tateyama::loopback::loopback_client loopback;
    sv.add_endpoint(loopback.endpoint());
    EXPECT_TRUE(sv.setup());
    EXPECT_TRUE(sv.start());
    //
    std::vector<tateyama::proto::debug::request::Logging_Level> level_list {
            tateyama::proto::debug::request::Logging_Level::Logging_Level_NOT_SPECIFIED,
            tateyama::proto::debug::request::Logging_Level::Logging_Level_INFO,
            tateyama::proto::debug::request::Logging_Level::Logging_Level_WARN,
            tateyama::proto::debug::request::Logging_Level::Logging_Level_ERROR
    };
    std::string message {"debug test message" };
    for (auto level : level_list) {
        logging_ok(loopback, level, message);
    }
    //
    const auto level = tateyama::proto::debug::request::Logging_Level::Logging_Level_INFO;
    logging_broken_message(loopback, level, message);
    logging_empty_req(loopback, level, message);
    //
    EXPECT_TRUE(sv.shutdown());
}

}  // namespace tateyama::debug
