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

 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <memory>
#include <map>
#include <cstdint>

#include "crypto/rsa.h"
#include "authentication_adapter.h"
#include "jwt/token_handler.h"
#include "crypto/key.h"

namespace tateyama::authentication::resource {

/**
 * @brief authentication_adapter_mock
 * @details This mock does not issue tokens, but verifies that it is signed with the correct private key.
 * And this mock authenticates encrypted_credential whose username/password combination are  admin-test and user-pass.
 */
class authentication_adapter_mock : public authentication_adapter {
  public:
    explicit authentication_adapter_mock(bool enabled) : enabled_(enabled) {
        if (enabled_) {
            rsa_ = std::make_unique<crypto::rsa>(crypto::base64_decode(crypto::private_key));
        }
    }

    [[nodiscard]] bool is_enabled() const noexcept override {
        return enabled_;
    }

    [[nodiscard]] std::optional<encryption_key_type> get_encryption_key() const override {
        if (enabled_) {
            return std::make_pair("RSA" , rsa_->public_key());
        }
        LOG(INFO) << "authentication service is not available";
        throw authentication_exception("authentication service is not available");
    }

    [[nodiscard]] std::optional<std::string> verify_token(std::string_view token) const override {
        if (enabled_) {
            auto rv = std::make_unique<jwt::token_handler>(token, rsa_->public_key())->tsurugi_auth_name();
            if (rv) {
                return rv;
            }
            LOG(INFO) << "token is malformed";
            throw authentication_exception("token is malformed");
        }
        LOG(INFO) << "authentication service is not available";
        throw authentication_exception("authentication service is not available");
    }

    [[nodiscard]] std::optional<std::string> verify_encrypted(std::string_view encrypted_credential) const override {
        if (enabled_) {
            std::vector<std::string> splited{};
            try {
                std::string credential = decrypt(encrypted_credential);
                splited = split(credential, '\n');
            } catch (boost::archive::iterators::dataflow_exception &ex) {
                LOG(INFO) << "encrypted credential is not encoded correctly";
                throw authentication_exception("encrypted credential is not encoded correctly");
            }

            if (splited.size() < 2) {
                LOG(INFO) << "encrypted credential is not formatted correctly";
                throw authentication_exception("encrypted credential is not formatted correctly");
            }
            try {
                std::string& username = splited.at(0);
                std::string& password = splited.at(1);
                if (auto itr = users_.find(username); itr != users_.end()) {
                    if (itr->second == password) {
                        return username;
                    }
                }
            } catch (std::runtime_error &ex) {
                LOG(INFO) << ex.what();
                throw authentication_exception(ex.what());
            }
        }
        LOG(INFO) << "authentication service is not available";
        throw authentication_exception("authentication service is not available");
    }

private:
    std::unique_ptr<crypto::rsa> rsa_{};
    bool enabled_;

    // user map for modk whose key is user and value is password
    std::map <std::string, std::string> users_{
        { "admin", "test" },
        { "user", "pass" }
    };

    [[nodiscard]] std::string decrypt(std::string_view encrypted) const {
        std::string decrypted{};
        rsa_->decrypt(crypto::base64_decode(encrypted), decrypted);
        return decrypted;
    }

    [[nodiscard]] static std::vector<std::string> split(const std::string& str, char del) {
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
};

}
