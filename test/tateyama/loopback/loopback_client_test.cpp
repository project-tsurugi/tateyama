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

#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/transactional_kvs_resource.h>
#include <tateyama/loopback/loopback_client.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::framework {

class loopback_client_test: public ::testing::Test, public test_utils::utility {
public:
    void SetUp() override {
        temporary_.prepare();
    }
    void TearDown() override {
        temporary_.clean();
    }
};

class data_channel_service: public tateyama::framework::service {
    static constexpr std::size_t default_writer_count = 32;

public:
    data_channel_service(int nchannel, int nwrite, int nloop) :
            nchannel_(nchannel), nwrite_(nwrite), nloop_(nloop) {
    }

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
        return "loopback:data_channel_service";
    }

    static constexpr std::string_view body_head = "body_head";

    static std::string channel_name(int ch) {
        return "ch" + std::to_string(ch);
    }

    static std::string channel_data(int ch, int w, int i) {
        return channel_name(ch) + "-w" + std::to_string(w) + "-" + std::to_string(i);
    }

    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) override {
        res->session_id(req->session_id());
        EXPECT_EQ(res->body_head(body_head), tateyama::status::ok);
        //
        for (int ch = 0; ch < nchannel_; ch++) {
            std::shared_ptr<tateyama::api::server::data_channel> channel;
            std::string name { channel_name(ch) };
            EXPECT_EQ(tateyama::status::ok, res->acquire_channel(name, channel, default_writer_count));
            for (int w = 0; w < nwrite_; w++) {
                std::shared_ptr<tateyama::api::server::writer> writer;
                EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
                for (int i = 0; i < nloop_; i++) {
                    std::string data { channel_data(ch, w, i) };
                    EXPECT_EQ(writer->write(data.c_str(), data.length()), tateyama::status::ok);
                    EXPECT_EQ(writer->commit(), tateyama::status::ok);
                }
                EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
            }
            EXPECT_EQ(res->release_channel(*channel), tateyama::status::ok);
        }
        EXPECT_EQ(res->body(req->payload()), tateyama::status::ok);
        return true;
    }

private:
    const int nchannel_;
    const int nwrite_;
    const int nloop_;
};

TEST_F(loopback_client_test, single) {
    const std::size_t session_id = 123;
    const std::size_t service_id = data_channel_service::tag;
    const std::string request { "loopback_test" };
    const int nchannel = 5;
    const int nwriter = 5;
    const int nloop = 10;
    //
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    tateyama::loopback::loopback_client loopback;
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg };
    sv.add_service(std::make_shared<framework::routing_service>());
    sv.add_service(std::make_shared<data_channel_service>(nchannel, nwriter, nloop));
    sv.add_endpoint(loopback.endpoint());
    ASSERT_TRUE(sv.start());
    //
    const auto response = loopback.request(session_id, service_id, request);
    EXPECT_EQ(response.session_id(), session_id);
    EXPECT_EQ(response.body_head(), data_channel_service::body_head);
    EXPECT_EQ(response.body(), request);
    auto &error_rec = response.error();
    EXPECT_EQ(error_rec.code(), 0);
    EXPECT_TRUE(error_rec.message().empty());
    //
    for (int ch = 0; ch < nchannel; ch++) {
        std::string name { std::move(data_channel_service::channel_name(ch)) };
        EXPECT_TRUE(response.has_channel(name));
        const std::vector<std::string> &ch_data = response.channel(name);
        EXPECT_EQ(ch_data.size(), nwriter * nloop);
        int idx = 0;
        for (int w = 0; w < nwriter; w++) {
            for (int i = 0; i < nloop; i++) {
                std::string data { std::move(data_channel_service::channel_data(ch, w, i)) };
                EXPECT_EQ(ch_data[idx++], data);
            }
        }
    }
    //
    EXPECT_TRUE(sv.shutdown());
}

TEST_F(loopback_client_test, multi_request) {
    const std::size_t session_id = 123;
    const std::size_t service_id = data_channel_service::tag;
    const std::string request { "loopback_test" };
    const int nchannel = 5;
    const int nwriter = 5;
    const int nloop = 10;
    const int nrequest = 10;
    //
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    tateyama::loopback::loopback_client loopback;
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg };
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.add_service(std::make_shared<data_channel_service>(nchannel, nwriter, nloop));
    sv.add_endpoint(loopback.endpoint());
    ASSERT_TRUE(sv.start());
    //
    for (int r = 0; r < nrequest; r++) {
        const auto response = loopback.request(session_id, service_id, request);
        EXPECT_EQ(response.session_id(), session_id);
        EXPECT_EQ(response.body_head(), data_channel_service::body_head);
        EXPECT_EQ(response.body(), request);
        auto &error_rec = response.error();
        EXPECT_EQ(error_rec.code(), 0);
        EXPECT_TRUE(error_rec.message().empty());
        //
        for (int ch = 0; ch < nchannel; ch++) {
            std::string name { std::move(data_channel_service::channel_name(ch)) };
            EXPECT_TRUE(response.has_channel(name));
            const std::vector<std::string> &ch_data = response.channel(name);
            EXPECT_EQ(ch_data.size(), nwriter * nloop);
            int idx = 0;
            for (int w = 0; w < nwriter; w++) {
                for (int i = 0; i < nloop; i++) {
                    std::string data { std::move(data_channel_service::channel_data(ch, w, i)) };
                    EXPECT_EQ(ch_data[idx++], data);
                }
            }
        }
    }
    //
    EXPECT_TRUE(sv.shutdown());
}

TEST_F(loopback_client_test, unknown_service_id) {
    const std::size_t session_id = 123;
    const std::size_t service_id = data_channel_service::tag;
    const std::size_t invalid_service_id = service_id + 1;
    const std::string request { "loopback_test" };
    //
    auto cfg = api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
    set_dbpath(*cfg);
    tateyama::loopback::loopback_client loopback;
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg };
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.add_service(std::make_shared<data_channel_service>(0, 0, 0));
    sv.add_endpoint(loopback.endpoint());
    ASSERT_TRUE(sv.start());
    //
    const auto response = loopback.request(session_id, invalid_service_id, request);
    EXPECT_EQ(response.session_id(), session_id);
    EXPECT_TRUE(response.body_head().empty());
    EXPECT_TRUE(response.body().empty());
    auto &error_rec = response.error();
    EXPECT_EQ(tateyama::proto::diagnostics::Code::SERVICE_UNAVAILABLE, error_rec.code());
    auto msg = error_rec.message();
    EXPECT_GT(msg.length(), 0);
    EXPECT_NE(msg.find(std::to_string(invalid_service_id)), std::string::npos);
    EXPECT_TRUE(sv.shutdown());
}

} // namespace tateyama::framework
