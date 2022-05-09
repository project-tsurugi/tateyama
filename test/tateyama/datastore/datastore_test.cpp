/*
 * Copyright 2018-2020 tsurugi project.
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
#include <tateyama/datastore/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

#include <gtest/gtest.h>

namespace tateyama::datastore {

using namespace std::literals::string_literals;

class datastore_test : public ::testing::Test {
public:
    class test_request : public api::server::request {
    public:
        test_request(
            std::size_t session_id,
            std::size_t service_id,
            std::string_view payload
        ) :
            session_id_(session_id),
            service_id_(service_id),
            payload_(payload)
        {}

        [[nodiscard]] std::size_t session_id() const override {
            return session_id_;
        }

        [[nodiscard]] std::size_t service_id() const override {
            return service_id_;
        }

        [[nodiscard]] std::string_view payload() const override {
            return payload_;
        }

        std::size_t session_id_{};
        std::size_t service_id_{};
        std::string payload_{};
    };

    class test_response : public api::server::response {
    public:
        test_response() = default;

        void session_id(std::size_t id) override {
            session_id_ = id;
        };
        void code(api::server::response_code code) override {}
        status body_head(std::string_view body_head) override {}
        status body(std::string_view body) override { body_ = body; }
        status acquire_channel(std::string_view name, std::shared_ptr<api::server::data_channel>& ch) override {}
        status release_channel(api::server::data_channel& ch) override {}
        status close_session() override {}

        std::size_t session_id_{};
        std::string body_{};
    };

};

using namespace std::string_view_literals;

TEST_F(datastore_test, basic) {
    auto cfg = api::configuration::create_configuration("");
    framework::server sv{framework::boot_mode::database_server, cfg};
    add_core_components(sv);
    sv.start();
    auto router = sv.find_service<framework::routing_service>();
    ASSERT_TRUE(router);
    ASSERT_EQ(framework::routing_service::tag, router->id());

    std::string str{};
    {
        ::tateyama::proto::datastore::request::Request rq{};
        rq.set_message_version(1);
        auto bb = rq.mutable_backup_begin();
        str = rq.SerializeAsString();
        rq.clear_backup_begin();
    }
    auto svrreq = std::make_shared<test_request>(10, datastore::service::bridge::tag, str);
    auto svrres = std::make_shared<test_response>();

    (*router)(svrreq, svrres);
    EXPECT_EQ(10, svrres->session_id_);
    auto& body = svrres->body_;
    std::vector<std::string> files{};
    ::tateyama::proto::datastore::response::BackupBegin bb{};
    ASSERT_TRUE(bb.ParseFromString(body));
    ASSERT_TRUE(bb.has_success());
    ASSERT_EQ(3, bb.success().files_size());
    for(auto&& f : bb.success().files()) {
        std::cout << "backup file: " << f << std::endl;
    }
    sv.shutdown();
}

}
