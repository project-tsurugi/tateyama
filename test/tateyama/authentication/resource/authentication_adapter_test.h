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

#include "tateyama/authentication/resource/authentication_adapter.h"
#include "tateyama/authentication/resource/crypto/rsa.h"
#include "tateyama/authentication/resource/jwt/token_handler.h"

#include "tateyama/authentication/resource/crypto/key.h"

namespace tateyama::authentication::resource {

class authentication_adapter_test : public tateyama::authentication::resource::authentication_adapter {
  public:
    explicit authentication_adapter_test(bool enabled) : enabled_(enabled) {
        if (enabled_) {
            rsa_ = std::make_unique<tateyama::authentication::resource::crypto::rsa>(tateyama::authentication::resource::crypto::base64_decode(tateyama::authentication::resource::crypto::private_key));
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
            auto handler = std::make_unique<tateyama::authentication::resource::jwt::token_handler>(token, rsa_->public_key());
            if (auto auth_name_opt = handler->tsurugi_auth_name(); auth_name_opt) {
                return auth_name_opt.value();
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> verify_encrypted(std::string_view encrypted_credential) const override {
        if (enabled_) {
            std::string credential = decrypt(encrypted_credential);
            std::vector<std::string> splited = split(credential, '\n');

            if (splited.size() < 2) {
                return std::nullopt;
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
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

private:
    std::unique_ptr<tateyama::authentication::resource::crypto::rsa> rsa_{};
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

}  // namespace tateyama::authentication::resource
