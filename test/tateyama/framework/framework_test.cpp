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
#include <tateyama/framework/component.h>
#include <tateyama/framework/repository.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/server.h>
#include <tateyama/framework/environment.h>

#include <gtest/gtest.h>

namespace tateyama::framework {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class framework_test : public ::testing::Test {
public:
    class test_resource : public resource {
    public:
        static constexpr id_type tag = 0;
        test_resource() = default;
        [[nodiscard]] id_type id() const noexcept override {
            return tag;
        }
        bool setup(environment&) override { return true; }
        bool start(environment&) override { return true; }
        bool shutdown(environment&) override { return true; }
    };

    class test_service : public service {
    public:
        static constexpr id_type tag = 0;

        test_service() = default;

        [[nodiscard]] id_type id() const noexcept override {
            return tag;
        }

        bool operator()(
            std::shared_ptr<request> req,
            std::shared_ptr<response> res) override {
            (void)req;
            (void)res;
            return true;
        }
        bool setup(environment&) override { return true; }
        bool start(environment&) override { return true; }
        bool shutdown(environment&) override { return true; }
    };
};

TEST_F(framework_test, server_api) {
    auto cfg = api::configuration::create_configuration("");
    server sv{boot_mode::database_server, cfg};
    //register_components(server);
    sv.add_resource(std::make_shared<test_resource>());
    sv.add_service(std::make_shared<test_service>());

    sv.start();
    sv.shutdown();
}

}
