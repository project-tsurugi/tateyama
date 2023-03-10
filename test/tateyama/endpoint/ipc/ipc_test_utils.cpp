/*
 * Copyright 2018-2023 tsurugi project.
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

#include <cstring>

#include "ipc_test_utils.h"
#include <sstream>
#include <istream>

namespace tateyama::api::endpoint::ipc {

void get_ipc_database_name(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
        std::string &ipc_database_name) {
    auto endpoint_config = cfg->get_section("ipc_endpoint");
    ASSERT_TRUE(endpoint_config);
    auto database_name_opt = endpoint_config->get<std::string>("database_name");
    ASSERT_TRUE(database_name_opt.has_value());
    ipc_database_name = database_name_opt.value();
    ASSERT_GT(ipc_database_name.size(), 0);
}

void get_ipc_max_session(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int &max_session) {
    auto endpoint_config = cfg->get_section("ipc_endpoint");
    ASSERT_TRUE(endpoint_config);
    auto threads_opt = endpoint_config->get<std::size_t>("threads");
    ASSERT_TRUE(threads_opt.has_value());
    max_session = static_cast<int>(threads_opt.value());
    ASSERT_GT(max_session, 0);
}

inline void add_length(std::vector<std::size_t> &vec, std::size_t len, std::size_t max) {
    if (len <= max) {
        vec.push_back(len);
    }
}

void make_power2_length_list(std::vector<std::size_t> &vec, std::size_t max) {
    vec.clear();
    add_length(vec, 1, max);
    add_length(vec, 2, max);
    for (unsigned int i = 2; i < 64; i++) {
        std::size_t len = 1UL << i;
        add_length(vec, len - 1, max);
        add_length(vec, len, max);
        add_length(vec, len + 1, max);
        if (len > max) {
            break;
        }
    }
}

void dump_length_list(std::vector<std::size_t> &vec) {
    for (size_t len : vec) {
        std::cout << len << " ";
    }
    std::cout << std::endl;
}

#define REAL_CHECK
static constexpr std::size_t max_ch = 256;

void make_dummy_message(std::size_t start_idx, std::size_t len, std::string &message) {
    message.resize(len);
#ifdef REAL_CHECK
    for (std::size_t i = 0, j = start_idx; i < len; i++, j++) {
        message[i] = static_cast<char>(j % max_ch);
    }
#endif
}

bool check_dummy_message(std::size_t start_idx, const std::string_view message) {
#ifdef REAL_CHECK
    size_t len = message.length();
    for (std::size_t i = 0, j = start_idx; i < len; i++, j++) {
        if (message[i] != static_cast<char>(j % max_ch)) {
            return false;
        }
    }
#endif
    return true;
}

void make_printable_dummy_message(std::size_t start_idx, std::size_t len, std::string &message) {
    static const std::string printable { "abcdefghijklmnopqrstuvwxyz" };
    message.resize(len);
    for (std::size_t i = 0, j = start_idx; i < len; i++, j++) {
        message[i] = printable[static_cast<int>(j % printable.size())];
    }
}

resultset_param::resultset_param(const std::string &text) {
    std::stringstream ss { text };
    std::getline(ss, name_, delim);
    std::string buf;
    for (int i = 0; std::getline(ss, buf, delim); i++) {
        std::size_t len = std::stoul(buf);
        switch(i) {
        case 0:
            write_nloop_ = len;
            break;
        case 1:
            nchannel_ = len;
            break;
        default:
            write_lens_.push_back(len);
            break;
        }
    }
}

void resultset_param::to_string(std::string &text) {
    text = name_;
    text += delim + std::to_string(write_nloop_);
    text += delim + std::to_string(nchannel_);
    for (std::size_t size : write_lens_) {
        text += delim + std::to_string(size);
    }
}

} // namespace tateyama::api::endpoint::ipc
