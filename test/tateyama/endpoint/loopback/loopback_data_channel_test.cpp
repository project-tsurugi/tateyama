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

class data_channel_service: public tateyama::framework::service {
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
        res->code(tateyama::api::server::response_code::success);
        res->body_head(body_head);
        //
        for (int ch = 0; ch < nchannel_; ch++) {
            std::shared_ptr<tateyama::api::server::data_channel> channel;
            std::string name { channel_name(ch) };
            EXPECT_EQ(tateyama::status::ok, res->acquire_channel(name, channel));
            for (int w = 0; w < nwrite_; w++) {
                std::shared_ptr<tateyama::api::server::writer> writer;
                EXPECT_EQ(tateyama::status::ok, channel->acquire(writer));
                for (int i = 0; i < nloop_; i++) {
                    std::string data { channel_data(ch, w, i) };
                    writer->write(data.c_str(), data.length());
                    writer->commit();
                }
                channel->release(*writer);
            }
            res->release_channel(*channel);
        }
        res->body(req->payload());
        return true;
    }

private:
    int nchannel_;
    int nwrite_;
    int nloop_;
};

class loopback_data_channel_test: public loopback_test_base {
public:
    const std::size_t session_id = 123;
    const std::size_t service_id = data_channel_service::tag;
    const std::string request { "loopback_test" };

    void check_response(int nchannel, int nwrite, int nloop, const tateyama::loopback::buffered_response &response) {
        EXPECT_EQ(response.session_id(), session_id);
        EXPECT_EQ(response.code(), tateyama::api::server::response_code::success);
        EXPECT_EQ(response.body_head(), data_channel_service::body_head);
        EXPECT_EQ(response.body(), request);
        //
        for (int ch = 0; ch < nchannel; ch++) {
            std::string name { std::move(data_channel_service::channel_name(ch)) };
            EXPECT_TRUE(response.has_channel(name));
            const std::vector<std::string> &ch_data = response.channel(name);
            EXPECT_EQ(ch_data.size(), nwrite * nloop);
            int idx = 0;
            for (int w = 0; w < nwrite; w++) {
                for (int i = 0; i < nloop; i++) {
                    std::string data { std::move(data_channel_service::channel_data(ch, w, i)) };
                    EXPECT_EQ(ch_data[idx++], data);
                }
            }
        }
    }

    void check_invalid_channel_name(const tateyama::loopback::buffered_response &response) {
        const std::string invalid_name { "invalid_name" };
        EXPECT_FALSE(response.has_channel(invalid_name));
        try {
            const std::vector<std::string> &ch_data = response.channel(invalid_name);
            FAIL();
            EXPECT_EQ(ch_data.size(), 0);
        } catch (std::invalid_argument &ex) {
            std::cout << ex.what() << std::endl;
            SUCCEED();
        }
    }
};

static const int nchannel = 2;
static const int nwrite = 2;
static const int nloop = 2;

TEST_F(loopback_data_channel_test, simple) {
    tateyama::loopback::loopback_client loopback;
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg_ };
    add_core_components(sv);
    sv.add_service(std::make_shared<data_channel_service>(nchannel, nwrite, nloop));
    sv.add_endpoint(loopback.endpoint());
    ASSERT_TRUE(sv.start());

    const auto response = loopback.request(session_id, service_id, request);
    check_response(nchannel, nwrite, nloop, response);
    check_invalid_channel_name(response);
    //
    EXPECT_TRUE(sv.shutdown());
}

TEST_F(loopback_data_channel_test, response_copy_move) {
    tateyama::loopback::loopback_client loopback;
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg_ };
    add_core_components(sv);
    sv.add_service(std::make_shared<data_channel_service>(nchannel, nwrite, nloop));
    sv.add_endpoint(loopback.endpoint());
    ASSERT_TRUE(sv.start());

    const auto response = loopback.request(session_id, service_id, request);
    check_response(nchannel, nwrite, nloop, response);
    const auto response_copy = response;  // NOLINT
    check_response(nchannel, nwrite, nloop, response_copy);
    const auto response_copy_ctor { response }; // NOLINT
    check_response(nchannel, nwrite, nloop, response_copy_ctor);
    //
    const auto response_move = std::move(response); // NOLINT
    check_response(nchannel, nwrite, nloop, response_move);
    check_response(nchannel, nwrite, nloop, response); // NOLINT
    const auto response_move_ctor { std::move(response) }; // NOLINT
    check_response(nchannel, nwrite, nloop, response_move_ctor);
    check_response(nchannel, nwrite, nloop, response_move); // NOLINT

    EXPECT_TRUE(sv.shutdown());
}

} // namespace tateyama::loopback
