/*
 * Copyright 2018-2025 Project Tsurugi.
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
#include <stdexcept>

#include <glog/logging.h>

#include "authentication_adapter_mock.h"

#include "bridge.h"

namespace tateyama::authentication::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment& env) {
    try {
        auto auth_conf = env.configuration()->get_section("authentication");
        auto enabled_option = auth_conf->get<bool>("enabled");
        bool enabled{false};
        if (enabled_option) {
            enabled = enabled_option.value();
        }
        if (enabled) {
            auto url_opt = auth_conf->get<std::string>("url");
            if (!url_opt) {
                LOG(ERROR) << "error in authentication setup: url is not provided by configuration";
                return false;
            }
            url_ = url_opt.value();
            auto request_timeout_opt = auth_conf->get<std::size_t>("request_timeout");
            if (!request_timeout_opt) {
                LOG(ERROR) << "error in authentication setup: request_timeout is not provided by configuration";
                return false;
            }
            request_timeout_ = request_timeout_opt.value();
        }
        authentication_adapter_ = std::make_unique<authentication_adapter_mock>(enabled);  // FIXME
    } catch (std::runtime_error &ex) {
        LOG(ERROR) << "error in authentication setup: " << ex.what();
        return false;
    }
    return true;
}

bool bridge::start(environment&) {
    return true;
}

bool bridge::shutdown(environment&) {
    return true;
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

std::string_view bridge::label() const noexcept {
    return component_label;
}

std::optional<std::string> bridge::get_encryption_key() const {
    if (enabled_) {
        if (auto key_opt = authentication_adapter_->get_encryption_key(); key_opt) {
            if (auto key = key_opt.value(); key.first == KEY_TYPE) {
                return key.second;
            }
        }
    }
    return std::nullopt;
}

static std::vector<std::string> split(std::string str, char del) {
    std::size_t first = 0;
    std::size_t last = str.find_first_of(del);

    std::vector<std::string> result;

    while (first < str.size()) {
        std::string subStr(str, first, last - first);

        result.push_back(subStr);

        first = last + 1;
        last = str.find_first_of(del, first);

        if (last == std::string::npos) {
            last = str.size();
        }
    }

    return result;
}

std::optional<std::string> bridge::verify_token(std::string_view token) const {
    if (enabled_) {
        return authentication_adapter_->verify_token(token);
    }
    return std::nullopt;
}

std::optional<std::string> bridge::verify_encrypted(std::string user_pass) const {
    if (enabled_) {
        std::vector<std::string> splited = split(user_pass, '.');

        if (splited.size() != 2) {
            LOG(INFO) << "user_password is invalid format";
            return std::nullopt;
        }
        try {
            return authentication_adapter_->verify_encrypted(splited.at(0), splited.at(1));
        } catch (std::runtime_error &ex) {
            LOG(INFO) << ex.what();
            return std::nullopt;
        }
    }
    return std::nullopt;
}

}
