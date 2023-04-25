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
#include <thread>

#include <tateyama/endpoint/loopback/loopback_data_channel.h>

#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class loopback_data_channel_test: public loopback_test_base {
};

TEST_F(loopback_data_channel_test, name) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);
    EXPECT_EQ(channel.name(), name);
}

TEST_F(loopback_data_channel_test, no_name) {
    const std::string name { };
    tateyama::common::loopback::loopback_data_channel channel(name);
    EXPECT_EQ(channel.name(), name);
}

TEST_F(loopback_data_channel_test, single_channel) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);

    std::shared_ptr<tateyama::api::server::writer> wrt;
    EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
    tateyama::common::loopback::loopback_data_writer *writer =
            dynamic_cast<tateyama::common::loopback::loopback_data_writer*>(wrt.get());
    {
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 0);
    }
    std::vector<std::string> test_data { "hello", "this is a pen" };
    {
        const std::string &data = test_data[0];
        EXPECT_EQ(writer->write(data.data(), data.length()), tateyama::status::ok);
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 0);
        EXPECT_EQ(channel.name(), name);
    }
    {
        EXPECT_EQ(writer->commit(), tateyama::status::ok);
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 1);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(channel.name(), name);
    }
    {
        const std::string &data = test_data[1];
        EXPECT_EQ(writer->write(data.data(), data.length()), tateyama::status::ok);
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 1);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(channel.name(), name);
    }
    {
        EXPECT_EQ(writer->commit(), tateyama::status::ok);
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 2);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(whole[1], test_data[1]);
        EXPECT_EQ(channel.name(), name);
    }
    {
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 2);
        // clear returned map
        whole.clear();
        EXPECT_EQ(whole.size(), 0);
        // still got same data
        channel.append_committed_data(whole);
        EXPECT_EQ(whole.size(), 2);
        EXPECT_EQ(whole[0], test_data[0]);
        EXPECT_EQ(whole[1], test_data[1]);
        EXPECT_EQ(channel.name(), name);
    }
    EXPECT_EQ(tateyama::status::ok, channel.release(*wrt));
}

TEST_F(loopback_data_channel_test, acquire_writer) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);

    std::shared_ptr<tateyama::api::server::writer> wrt;
    EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
    EXPECT_EQ(tateyama::status::ok, channel.release(*wrt));
}

TEST_F(loopback_data_channel_test, acquire_writer_without_release) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);

    std::shared_ptr<tateyama::api::server::writer> wrt;
    EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
}

TEST_F(loopback_data_channel_test, acquire_writer_dual_release) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);

    std::shared_ptr<tateyama::api::server::writer> wrt;
    EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
    EXPECT_EQ(tateyama::status::ok, channel.release(*wrt));
    EXPECT_EQ(tateyama::status::not_found, channel.release(*wrt));
}

TEST_F(loopback_data_channel_test, dual_acquire_writer) {
    const std::string name { "channelX" };
    std::shared_ptr<tateyama::api::server::writer> wrt1;
    std::shared_ptr<tateyama::api::server::writer> wrt2;
    {
        tateyama::common::loopback::loopback_data_channel channel(name);
        EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt1));
        EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt2));
        EXPECT_EQ(tateyama::status::ok, channel.release(*wrt1));
        EXPECT_EQ(tateyama::status::ok, channel.release(*wrt2));
    }
    {
        tateyama::common::loopback::loopback_data_channel channel(name);
        EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt1));
        EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt2));
        EXPECT_EQ(tateyama::status::ok, channel.release(*wrt2));
        EXPECT_EQ(tateyama::status::ok, channel.release(*wrt1));
    }
}

TEST_F(loopback_data_channel_test, multi_acquire_writer) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);
    std::vector<std::shared_ptr<tateyama::api::server::writer>> writers { };
    const int nwriter = 10;
    for (int i = 0; i < nwriter; i++) {
        std::shared_ptr<tateyama::api::server::writer> wrt;
        EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
        writers.emplace_back(std::move(wrt));
    }
    for (auto &wrt : writers) {
        EXPECT_EQ(tateyama::status::ok, channel.release(*wrt));
    }
}

inline void dummy_message(int i, int j, std::string &s) {
    s = std::to_string(i) + "-" + std::to_string(j);
}

TEST_F(loopback_data_channel_test, parallel_writer) {
    const std::string name { "channelX" };
    tateyama::common::loopback::loopback_data_channel channel(name);
    const int nwriter = 10;
    const int write_loop = 1000;
    std::vector<std::thread> threads { };
    threads.reserve(nwriter);
    for (int i = 0; i < nwriter; i++) {
        threads.emplace_back([this, i, &channel] {
            std::string s { };
            std::shared_ptr<tateyama::api::server::writer> wrt;
            EXPECT_EQ(tateyama::status::ok, channel.acquire(wrt));
            tateyama::common::loopback::loopback_data_writer *writer =
                    dynamic_cast<tateyama::common::loopback::loopback_data_writer*>(wrt.get());
            for (int k = 0; k < write_loop; k++) {
                dummy_message(i, k, s);
                wrt->write(s.c_str(), s.length());
                wrt->commit();
                EXPECT_EQ(writer->committed_data().size(), k + 1);
            }
            EXPECT_EQ(tateyama::status::ok, channel.release(*wrt));
        });
    }
    for (std::thread &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
    //
    std::set<std::string> resultset { };
    {
        std::vector<std::string> whole { };
        channel.append_committed_data(whole);
        for (const auto &s : whole) {
            resultset.emplace(s);
        }
    }
    //
    EXPECT_EQ(resultset.size(), nwriter * write_loop);
    std::string s { };
    const auto cend = resultset.cend();
    for (int i = 0; i < nwriter; i++) {
        for (int k = 0; k < write_loop; k++) {
            dummy_message(i, k, s);
            EXPECT_NE(resultset.find(s), cend);
        }
    }
}

} // namespace tateyama::api::endpoint::loopback
