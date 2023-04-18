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
#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class echo_service: public tateyama::framework::service {
public:
    static constexpr tateyama::framework::component::id_type tag = 1234;
    [[nodiscard]] tateyama::framework::component::id_type id() const noexcept override {
        return tag;
    }

    bool start(tateyama::framework::environment&) override {
        return true;
    }
    bool setup(tateyama::framework::environment&) override {
        return true;
    }
    bool shutdown(tateyama::framework::environment&) override {
        return true;
    }

    [[nodiscard]] std::string_view label() const noexcept override {
        return "loopback:echo_service";
    }

    static constexpr std::string_view body_head = "body_head";

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        std::cout << "HERE! : " << req->payload() << std::endl;
        //
        res->session_id(req->session_id());
        res->code(tateyama::api::server::response_code::success);
        res->body_head(body_head);
        res->body(req->payload());
        return true;
    }
};

class loopback_echo_test: public loopback_test_base {
};

TEST_F(loopback_echo_test, simple) {
    const std::size_t session_id = 123;
    const std::size_t service_id = echo_service::tag;
    const std::string request { "loopback_test" };
    //
    tateyama::loopback::loopback_client loopback;
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg_ };
    add_core_components(sv);
    sv.add_service(std::make_shared<echo_service>());
    sv.add_endpoint(loopback.endpoint());
    ASSERT_TRUE(sv.start());

    // NOTE: use 'const auto' to avoid calling response.body("txt") etc.
    const auto response = loopback.request(session_id, service_id, request);
    EXPECT_EQ(response.session_id(), session_id);
    EXPECT_EQ(response.code(), tateyama::api::server::response_code::success);
    EXPECT_EQ(response.body_head(), echo_service::body_head);
    EXPECT_EQ(response.body(), request);
    //
    EXPECT_TRUE(sv.shutdown());
}

}
// namespace tateyama::loopback
