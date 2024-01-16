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

#include <cstring>

#include "ipc_test_utils.h"
#include <sstream>
#include <istream>

namespace tateyama::endpoint::ipc {

void get_ipc_database_name(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
        std::string &ipc_database_name) {
    auto endpoint_config = cfg->get_section("ipc_endpoint");
    auto database_name_opt = endpoint_config->get<std::string>("database_name");
    ipc_database_name = database_name_opt.value();
}

void get_ipc_max_session(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int &max_session) {
    auto endpoint_config = cfg->get_section("ipc_endpoint");
    auto threads_opt = endpoint_config->get<std::size_t>("threads");
    max_session = static_cast<int>(threads_opt.value());
}

inline void add_length(std::vector<std::size_t> &vec, const std::size_t len, const std::size_t max) {
    if (len <= max) {
        vec.push_back(len);
    }
}

void make_power2_length_list(std::vector<std::size_t> &vec, const std::size_t max) {
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

void make_dummy_message(const std::size_t start_idx, const std::size_t len, std::string &message) {
    message.resize(len);
#ifdef REAL_CHECK
    for (std::size_t i = 0, j = start_idx; i < len; i++, j++) {
        message[i] = static_cast<char>(j % max_ch);
    }
#endif
}

bool check_dummy_message(const std::size_t start_idx, const std::string_view message) {
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

static const std::string msg_params_delim = ":";
static const std::string msg_params_end = ".";

void params_to_string(const std::size_t len, const std::vector<std::size_t> &msg_params, std::string &text) {
    text = std::to_string(len);
    for (std::size_t param : msg_params) {
        text += ":";
        text += std::to_string(param);
    }
    text += msg_params_end;
}

std::size_t get_message_len(const std::string_view message) {
    auto pos = message.find_first_of(msg_params_delim);
    if (pos > 0) {
        return std::stoul(message.substr(0, pos).data());
    }
    return 0;
}

std::string_view get_message_part(const std::string_view message) {
    auto pos = message.find_first_of(msg_params_end);
    if (pos > 0) {
        return message.substr(0, pos + msg_params_end.length());
    }
    return "";
}

std::string_view get_message_part_without_len(const std::string_view message) {
    auto pos_start = message.find_first_of(msg_params_delim);
    auto pos_end = message.find_first_of(msg_params_end);
    if (pos_start > 0 && pos_end > 0) {
        auto start = pos_start;
        auto end = pos_end + msg_params_end.length();
        auto n = end - start;
        return message.substr(start, n);
    }
    return "";
}

void make_dummy_message(const std::string &part, const std::size_t len, std::string &message) {
    std::size_t endlen = len - part.length();
    message = "";
    message.reserve(len);
#ifdef REAL_CHECK
    while (message.length() < endlen) {
        message += part;
    }
    if (message.length() < len) {
        message += part.substr(0, len - message.length());
    }
#endif
}

bool check_dummy_message(const std::string_view message) {
#ifdef REAL_CHECK
    std::size_t len = get_message_len(message);
    if (message.length() != len) {
        return false;
    }
    std::string_view part = get_message_part(message);
    if (part.length() == 0) {
        return false;
    }
    std::size_t endlen = message.length() - part.length();
    std::size_t start = 0;
    for (; start < endlen; start += part.length()) {
        if (part != message.substr(start, part.length())) {
            return false;
        }
    }
    std::size_t remain = message.length() - start;
    if (remain > 0) {
        return (part.substr(0, remain) == message.substr(start, remain));
    }
#endif
    return true;
}

resultset_param::resultset_param(const std::string &text) {
    std::stringstream ss { text };
    std::getline(ss, name_, delim);
    std::string buf;
    for (int i = 0; std::getline(ss, buf, delim); i++) {
        std::size_t len = std::stoul(buf);
        switch (i) {
        case 0:
            write_nloop_ = len;
            break;
        case 1:
            value_ = len;
            break;
        default:
            write_lens_.push_back(len);
            break;
        }
    }
}

void resultset_param::to_string(std::string &text) noexcept {
    text = name_;
    text += delim + std::to_string(write_nloop_);
    text += delim + std::to_string(value_);
    for (std::size_t size : write_lens_) {
        text += delim + std::to_string(size);
    }
}

} // namespace tateyama::endpoint::ipc
