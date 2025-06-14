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
#include <iostream>
#include <string>
#include <memory>
#include <map>

#include "crypto/rsa.h"
#include "authentication_adapter.h"
#include "jwt/token_handler.h"

namespace tateyama::authentication::resource {

/**
 * @brief authentication_adapter_mock
 * @details In order to make this mock work, set the environment variable TSURUGI_JWT_SECRET_KEY to the RSA private key string in pem format
 * with CR codes removed and start the tsurugidb. This mock does not issue tokens, but verifies that it is signed with the correct private key.
 */
class authentication_adapter_mock : public authentication_adapter {
  public:
    explicit authentication_adapter_mock(bool enabled) : enabled_(enabled) {
        if (enabled_) {
            if (const char* key = std::getenv("TSURUGI_JWT_SECRET_KEY")) {
                rsa_ = std::make_unique<crypto::rsa>(key);
            } else {
                throw std::runtime_error("error in authentication setup: TSURUGI_JWT_SECRET_KEY is not provided by environment variable");
            }
        }
    }

    [[nodiscard]] bool is_enabled() const noexcept override {
        return enabled_;
    }

    [[nodiscard]] std::optional<encryption_key_type> get_encryption_key() const override {
        if (enabled_) {
            return std::make_pair("RSA" , rsa_->public_key());
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> verify_token(std::string_view token) const override {
        if (enabled_) {
            auto handler = std::make_unique<jwt::token_handler>(token, rsa_->public_key());
            if (auto auth_name_opt = handler->tsurugi_auth_name(); auth_name_opt) {
                return auth_name_opt.value();
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> verify_encrypted(std::string_view username, std::string_view password) const override {
        if (enabled_) {
            std::string username_p;
            std::string password_p;
            rsa_->decrypt(crypto::base64_decode(username), username_p);
            rsa_->decrypt(crypto::base64_decode(password), password_p);

            if (auto itr = users_.find(username_p); itr != users_.end()) {
                if (itr->second == password_p) {
                    return username_p;
                }
            }
        }
        return std::nullopt;
    }

private:
    std::unique_ptr<crypto::rsa> rsa_{};
    bool enabled_;

    // user map for modk whose key is user and value is password
    std::map <std::string, std::string> users_{
        { "admin", "test" },
        { "user", "pass" }
    };
};

}
