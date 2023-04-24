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

#include <tateyama/endpoint/loopback/loopback_response.h>

#include "loopback_test_base.h"

namespace tateyama::api::endpoint::loopback {

class loopback_response_test: public loopback_test_base {
};

TEST_F(loopback_response_test, empty) {
    tateyama::common::loopback::loopback_response response { };
    EXPECT_EQ(response.session_id(), 0);
    EXPECT_EQ(response.code(), tateyama::api::server::response_code::success);
    EXPECT_EQ(response.body_head().length(), 0);
    EXPECT_EQ(response.body().length(), 0);
    EXPECT_FALSE(response.has_channel(""));
    EXPECT_FALSE(response.has_channel("name"));
}

TEST_F(loopback_response_test, set_get) {
    tateyama::common::loopback::loopback_response response { };
    //
    const std::size_t session_id = 123;
    response.session_id(session_id);
    EXPECT_EQ(response.session_id(), session_id);
    //
    tateyama::api::server::response_code codes[] = { tateyama::api::server::response_code::success,
            tateyama::api::server::response_code::application_error, tateyama::api::server::response_code::io_error,
            tateyama::api::server::response_code::unknown };
    for (const auto code : codes) {
        response.code(code);
        EXPECT_EQ(response.code(), code);
    }
    response.code(tateyama::api::server::response_code::success);
    //
    EXPECT_EQ(response.body_head().length(), 0);
    EXPECT_EQ(response.body_head(""), tateyama::status::ok);
    EXPECT_EQ(response.body_head().length(), 0);
    const std::string body_head { "body_head" };
    EXPECT_EQ(response.body_head(body_head), tateyama::status::ok);
    EXPECT_EQ(response.body_head(), body_head);
    //
    EXPECT_EQ(response.body().length(), 0);
    EXPECT_EQ(response.body(""), tateyama::status::ok);
    EXPECT_EQ(response.body().length(), 0);
    const std::string body { "body message" };
    EXPECT_EQ(response.body(body), tateyama::status::ok);
    EXPECT_EQ(response.body(), body);
    EXPECT_EQ(response.body_head(), body_head);
    //
    EXPECT_EQ(response.close_session(), tateyama::status::ok);
    //
    EXPECT_EQ(response.session_id(), session_id);
    EXPECT_EQ(response.code(), tateyama::api::server::response_code::success);
    EXPECT_EQ(response.body(), body);
    EXPECT_EQ(response.body_head(), body_head);
}

TEST_F(loopback_response_test, single) {
    const std::size_t session_id = 123;
    const std::string body_head { "body_head" };
    const std::string body { "body message" };
    const std::string name { "channelA" };
    const std::vector<std::string> test_data = { "hello", "this is a pen" };
    //
    tateyama::common::loopback::loopback_response response { };
    response.session_id(session_id);
    response.code(tateyama::api::server::response_code::success);
    EXPECT_EQ(response.body_head(body_head), tateyama::status::ok);
    //
    EXPECT_FALSE(response.has_channel(name));
    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    //
    std::shared_ptr<tateyama::api::server::writer> writer;
    EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
    //
    for (const auto &s : test_data) {
        EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
        EXPECT_EQ(writer->commit(), tateyama::status::ok);
    }
    EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
    //
    EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name)); // NOTE: it's released, but has data
    //
    EXPECT_EQ(response.body(body), tateyama::status::ok);
    //
    EXPECT_EQ(response.session_id(), session_id);
    EXPECT_EQ(response.code(), tateyama::api::server::response_code::success);
    EXPECT_EQ(response.body_head(), body_head);
    EXPECT_EQ(response.body(), body);
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    {
        response.all_committed_data(data_map);
        EXPECT_EQ(data_map.size(), 1);
        EXPECT_NE(data_map.find(name), data_map.cend());
        std::vector<std::string> &result = data_map[name];
        EXPECT_EQ(result.size(), test_data.size());
        for (int i = 0; i < result.size(); i++) {
            EXPECT_EQ(result[i], test_data[i]);
        }
    }
    // re-get committed data after clearing response
    {
        data_map.clear();
        response.all_committed_data(data_map);
        EXPECT_EQ(data_map.size(), 1);
        EXPECT_NE(data_map.find(name), data_map.cend());
        std::vector<std::string> &result = data_map[name];
        EXPECT_EQ(result.size(), test_data.size());
        for (int i = 0; i < result.size(); i++) {
            EXPECT_EQ(result[i], test_data[i]);
        }
    }
}

TEST_F(loopback_response_test, dual_acqurie_channel) {
    const std::string name { "channelA" };
    tateyama::common::loopback::loopback_response response { };
    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_FALSE(response.has_channel(name));
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    // re-acquire of acquired channel is prohibited
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::not_found);
    EXPECT_TRUE(response.has_channel(name));
    EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
}

TEST_F(loopback_response_test, twice_acqurie_channel) {
    const std::string name { "channelA" };
    tateyama::common::loopback::loopback_response response { };
    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_FALSE(response.has_channel(name));
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    // re-acquire of already released channel is allowed
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
}

TEST_F(loopback_response_test, reacqurie_channel) {
    const std::string name { "channelA" };
    const std::vector<std::string> test_data = { "hello", "this is a pen" };
    const std::vector<std::string> test_data2 = { "good night", "it's fine today" };
    //
    tateyama::common::loopback::loopback_response response { };
    {
        EXPECT_FALSE(response.has_channel(name));
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
        EXPECT_TRUE(response.has_channel(name));
        //
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
        //
        for (const auto &s : test_data) {
            EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
            EXPECT_EQ(writer->commit(), tateyama::status::ok);;
        }
        EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
        //
        EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
        EXPECT_TRUE(response.has_channel(name)); // NOTE: it's released, but has data
    }
    {
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
        EXPECT_TRUE(response.has_channel(name));
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
        for (const auto &s : test_data2) {
            EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
            EXPECT_EQ(writer->commit(), tateyama::status::ok);;
        }
        EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
        EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
        EXPECT_TRUE(response.has_channel(name)); // NOTE: it's released, but has data
    }
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    {
        response.all_committed_data(data_map);
        EXPECT_EQ(data_map.size(), 1);
        EXPECT_NE(data_map.find(name), data_map.cend());
        std::vector<std::string> &result = data_map[name];
        EXPECT_EQ(result.size(), test_data.size() + test_data2.size());
        int i = 0;
        for (; i < test_data.size(); i++) {
            EXPECT_EQ(result[i], test_data[i]);
        }
        for (int j = 0; j < test_data2.size(); i++, j++) {
            EXPECT_EQ(result[i], test_data2[j]);
        }
    }
}

TEST_F(loopback_response_test, not_release_writer) {
    const std::string name { "channelA" };
    const std::vector<std::string> test_data = { "hello", "this is a pen" };
    //
    tateyama::common::loopback::loopback_response response { };
    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    std::shared_ptr<tateyama::api::server::writer> writer;
    EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
    for (const auto &s : test_data) {
        EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
        EXPECT_EQ(writer->commit(), tateyama::status::ok);;
    }
    // not call release() writer:
    // EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
    // NOTE: response.release_channel(*channel) should call release(unreleased_writer) automatically
    EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), 1);
    EXPECT_NE(data_map.find(name), data_map.cend());
    std::vector<std::string> &result = data_map[name];
    EXPECT_EQ(result.size(), test_data.size());
    for (int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], test_data[i]);
    }
}

TEST_F(loopback_response_test, not_release_channel) {
    const std::string name { "channelA" };
    const std::vector<std::string> test_data = { "hello", "this is a pen" };
    //
    tateyama::common::loopback::loopback_response response { };
    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    std::shared_ptr<tateyama::api::server::writer> writer;
    EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
    for (const auto &s : test_data) {
        EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
        EXPECT_EQ(writer->commit(), tateyama::status::ok);;
    }
    EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
    // not call release_channel
    // EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    // got data from not released channel
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), 1);
    EXPECT_NE(data_map.find(name), data_map.cend());
    std::vector<std::string> &result = data_map[name];
    EXPECT_EQ(result.size(), test_data.size());
    for (int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], test_data[i]);
    }
}

TEST_F(loopback_response_test, dual_channel) {
    const std::vector<std::string> names = { "channelA", "channelB" };
    std::map<std::string, std::vector<std::string>> test_data { };
    test_data[names[0]] = { "hello", "this is a pen" };
    test_data[names[1]] = { "good night", "it's fine today" };
    std::vector<std::shared_ptr<tateyama::api::server::data_channel>> channels { };
    //
    tateyama::common::loopback::loopback_response response { };
    for (const auto &name : names) {
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
        channels.emplace_back(std::move(channel));
    }
    for (auto &channel : channels) {
        std::shared_ptr<tateyama::api::server::writer> writer;
        EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
        auto data_channel = dynamic_cast<tateyama::common::loopback::loopback_data_channel*>(channel.get());
        for (const auto &s : test_data[data_channel->name()]) {
            EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
            EXPECT_EQ(writer->commit(), tateyama::status::ok);;
        }
        EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
        EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    }
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), names.size());
    //
    for (const auto &name : names) {
        EXPECT_TRUE(response.has_channel(name));
        EXPECT_NE(data_map.find(name), data_map.cend());
        std::vector<std::string> &result = data_map[name];
        EXPECT_EQ(result.size(), test_data.size());
        for (int i = 0; i < result.size(); i++) {
            EXPECT_EQ(result[i], test_data[name][i]);
        }
    }
}

TEST_F(loopback_response_test, empty_channel_name) {
    const std::string name {  };
    const std::string unknown_name { "channelXXX" };
    const std::vector<std::string> test_data = { "hello", "this is a pen" };
    //
    tateyama::common::loopback::loopback_response response { };
    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    EXPECT_FALSE(response.has_channel(unknown_name));
    std::shared_ptr<tateyama::api::server::writer> writer;
    EXPECT_EQ(channel->acquire(writer), tateyama::status::ok);
    for (const auto &s : test_data) {
        EXPECT_EQ(writer->write(s.c_str(), s.length()), tateyama::status::ok);
        EXPECT_EQ(writer->commit(), tateyama::status::ok);;
    }
    EXPECT_EQ(channel->release(*writer), tateyama::status::ok);
    EXPECT_EQ(response.release_channel(*channel), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    EXPECT_FALSE(response.has_channel(unknown_name));
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), 1);
    EXPECT_NE(data_map.find(name), data_map.cend());
    std::vector<std::string> &result = data_map[name];
    EXPECT_EQ(result.size(), test_data.size());
    for (int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], test_data[i]);
    }
    //
    EXPECT_EQ(response.close_session(), tateyama::status::ok);
    EXPECT_TRUE(response.has_channel(name));
    EXPECT_FALSE(response.has_channel(unknown_name));
}

inline void dummy_message(int i, int j, std::string &s) {
    s = std::to_string(i) + "-" + std::to_string(j);
}

TEST_F(loopback_response_test, parallel_writer) {
    tateyama::common::loopback::loopback_response response { };
    const std::string name { "channelA" };
    const int nwriter = 10;
    const int write_loop = 1000;
    std::vector<std::thread> threads { };
    threads.reserve(nwriter);

    std::shared_ptr<tateyama::api::server::data_channel> channel;
    EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
    for (int i = 0; i < nwriter; i++) {
        threads.emplace_back([this, i, &channel] {
            std::string s { };
            std::shared_ptr<tateyama::api::server::writer> wrt;
            EXPECT_EQ(tateyama::status::ok, channel->acquire(wrt));
            tateyama::common::loopback::loopback_data_writer *writer =
                    dynamic_cast<tateyama::common::loopback::loopback_data_writer*>(wrt.get());
            for (int k = 0; k < write_loop; k++) {
                dummy_message(i, k, s);
                wrt->write(s.c_str(), s.length());
                wrt->commit();
                EXPECT_EQ(writer->committed_data().size(), k + 1);
            }
            EXPECT_EQ(tateyama::status::ok, channel->release(*wrt));
        });
    }
    EXPECT_EQ(threads.size(), nwriter);
    for (std::thread &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
    EXPECT_EQ(tateyama::status::ok, response.release_channel(*channel));
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), 1);
    EXPECT_NE(data_map.find(name), data_map.cend());
    //
    std::set<std::string> resultset { };
    const auto cend = resultset.cend();
    for (const auto &d : data_map[name]) {
        resultset.emplace(d);
    }
    for (int i = 0; i < nwriter; i++) {
        std::string s { };
        for (int k = 0; k < write_loop; k++) {
            dummy_message(i, k, s);
            EXPECT_NE(resultset.find(s), cend);
        }
    }
}

TEST_F(loopback_response_test, parallel_channel) {
    tateyama::common::loopback::loopback_response response { };
    const int nchannel = 10;
    const int write_loop = 1000;
    std::vector<std::thread> threads { };
    threads.reserve(nchannel);
    for (int i = 0; i < nchannel; i++) {
        threads.emplace_back([this, i, &response] {
            std::string name { std::to_string(i) };
            std::shared_ptr<tateyama::api::server::data_channel> channel;
            EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
            std::string s { };
            std::shared_ptr<tateyama::api::server::writer> wrt;
            EXPECT_EQ(tateyama::status::ok, channel->acquire(wrt));
            tateyama::common::loopback::loopback_data_writer *writer =
                    dynamic_cast<tateyama::common::loopback::loopback_data_writer*>(wrt.get());
            for (int k = 0; k < write_loop; k++) {
                dummy_message(i, k, s);
                wrt->write(s.c_str(), s.length());
                wrt->commit();
                EXPECT_EQ(writer->committed_data().size(), k + 1);
            }
            EXPECT_EQ(tateyama::status::ok, channel->release(*wrt));
            EXPECT_EQ(tateyama::status::ok, response.release_channel(*channel));
        });
    }
    EXPECT_EQ(threads.size(), nchannel);
    for (std::thread &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), nchannel);
    //
    for (int i = 0; i < nchannel; i++) {
        std::string name { std::to_string(i) };
        EXPECT_NE(data_map.find(name), data_map.cend());
        std::set<std::string> resultset { };
        const auto cend = resultset.cend();
        for (const auto &d : data_map[name]) {
            resultset.emplace(d);
        }
        std::string s { };
        for (int k = 0; k < write_loop; k++) {
            dummy_message(i, k, s);
            EXPECT_NE(resultset.find(s), cend);
        }
    }
}

inline void dummy_message(int i, int j, int k, std::string &s) {
    s = std::to_string(i) + "-" + std::to_string(j) + "-" + std::to_string(k);
}

TEST_F(loopback_response_test, parallel_channel_and_wrier) {
    tateyama::common::loopback::loopback_response response { };
    const int nchannel = 10;
    const int nwriter = 10;
    const int nthread = nchannel * nwriter;
    const int write_loop = 100;
    std::vector<std::shared_ptr<tateyama::api::server::data_channel>> channels { };
    std::vector<std::vector<std::shared_ptr<tateyama::api::server::writer>>> writers_map { };

    std::vector<std::thread> threads { };
    threads.reserve(nthread);
    for (int i = 0; i < nchannel; i++) {
        std::string name { std::to_string(i) };
        std::shared_ptr<tateyama::api::server::data_channel> channel;
        EXPECT_EQ(response.acquire_channel(name, channel), tateyama::status::ok);
        channels.emplace_back(channel);
        writers_map.emplace_back(std::vector<std::shared_ptr<tateyama::api::server::writer>>());
        for (int j = 0; j < nwriter; j++) {
            std::shared_ptr<tateyama::api::server::writer> wrt;
            EXPECT_EQ(tateyama::status::ok, channel->acquire(wrt));
            writers_map[i].emplace_back(std::move(wrt));
        }
    }
    for (int i = 0; i < nchannel; i++) {
        auto &channel = channels[i];
        for (int j = 0; j < nwriter; j++) {
            auto &writer = writers_map[i][j];
            threads.emplace_back([this, i, j, &writer] {
                std::string s { };
                tateyama::common::loopback::loopback_data_writer *data_writer =
                        dynamic_cast<tateyama::common::loopback::loopback_data_writer*>(writer.get());
                for (int k = 0; k < write_loop; k++) {
                    dummy_message(i, j, k, s);
                    EXPECT_EQ(tateyama::status::ok, writer->write(s.c_str(), s.length()));
                    EXPECT_EQ(tateyama::status::ok, writer->commit());
                    EXPECT_EQ(data_writer->committed_data().size(), k + 1);
                }
            });
        }
    }
    EXPECT_EQ(threads.size(), nthread);
    for (std::thread &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
    for (int i = 0; i < nchannel; i++) {
        auto &channel = channels[i];
        for (int j = 0; j < nwriter; j++) {
            auto &writer = writers_map[i][j];
            EXPECT_EQ(tateyama::status::ok, channel->release(*writer));
        }
        EXPECT_EQ(tateyama::status::ok, response.release_channel(*channel));
    }
    //
    std::map<std::string, std::vector<std::string>> data_map { };
    response.all_committed_data(data_map);
    EXPECT_EQ(data_map.size(), nchannel);
    //
    for (int i = 0; i < nchannel; i++) {
        std::string name { std::to_string(i) };
        EXPECT_NE(data_map.find(name), data_map.cend());
        std::set<std::string> resultset { };
        const auto cend = resultset.cend();
        for (const auto &d : data_map[name]) {
            resultset.emplace(d);
        }
        std::string s { };
        for (int j = 0; j < nwriter; j++) {
            for (int k = 0; k < write_loop; k++) {
                dummy_message(i, j, k, s);
                EXPECT_NE(resultset.find(s), cend);
            }
        }
    }
}

} // namespace tateyama::api::endpoint::loopback