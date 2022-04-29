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

class repository_test : public ::testing::Test {

};

using namespace std::string_view_literals;

class test_resource0 : public resource {
public:
    static constexpr id_type tag = 0;
    test_resource0() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
};

class test_resource1 : public resource {
public:
    static constexpr id_type tag = 1;
    test_resource1() = default;
    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }
    void setup(environment&) override {}
    void start(environment&) override {}
    void shutdown(environment&) override {}
};

TEST_F(repository_test, basic) {
    repository<resource> rep{};
    auto res0 = std::make_shared<test_resource0>();
    auto res1 = std::make_shared<test_resource1>();
    rep.add(res0);
    rep.add(res1);
    int cnt = 0;
    rep.each([&](resource& res){
        ASSERT_EQ(cnt, res.id());
        ++cnt;
    });

    EXPECT_EQ(res1, rep.find_by_id(1));
    EXPECT_EQ(res0, rep.find_by_id(0));
    EXPECT_EQ(res0, rep.find<test_resource0>());
    EXPECT_EQ(res1, rep.find<test_resource1>());
}

}
