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
#include <iostream>
#include <string>
#include <string_view>
#include <memory>
#include <map>
#include <cstdint>

#include "crypto/rsa.h"
#include "authentication_adapter.h"
#include "jwt/token_handler.h"

namespace tateyama::authentication::resource {

using namespace std::string_view_literals;

constexpr static std::string_view key{"-----BEGIN PRIVATE KEY-----MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCVXTeYEcg4rrQFWeBRuil7+ZQ4O5EJqh5SNyFiULfRNqVqIUge/gsOmP9ksPYW5PQ9ILg3OJq6x3guEKLPIMjpN7fXlZ3GojK68U9bI62Qu7zevmduuM4mOJ5fClPPsq85tt2MXlxdYkXKxJT+F2k/s3XhhTGJUmdDodk3NThHrloPuRJIz9zf5fGzWC0kRL0IcTAZjVIGBE/xCW9ZnoEPeMxUGbVBjZwzUoiSBMJ1EwR1FiSEzVFs4NIQaDRbvAeK2JAhTw7vcHmkhCk7P2xUoTHujYjL391qjlKGdsTsZgq9L1Z6qKWnXZrJJknKI+MgqxSDWDnuwDQQL7ip2EaVAgMBAAECggEAQsQDaM9yD5xQTiAJvJ6ZkphSn/xIbeiEWz3Xh2oLcNKbiGBOK8RlTuYnK2xK5Jr9biGlFtIPoDQvzW+UR0AhbtaAMDbp6vNv986MKXI+UHcLCwpTk9O6Gq2uZU9pfWsjFopeaDN52ChoiCXtb9MpMddXdzKhnP+ft0SuoxYADVH8fXuY2xZKebnTCy6O2DMUwBwDU5eshnBMhOjpFglwCBayZMfB2+xIFi0bo3FjwIXW4crMjra68JezGFfvlH2gztCoL4RyjTQzNnWBji4slf91Kf9xSY/M6fnA+Z6DGkG6OIFjU9/jDTfWQENMEdPpH3Yh//4W8N7znPGHo/OsNQKBgQC96SovBg01RFtpWuRwUz7Wl/Rl5ZOcG9TuAF9/m3gUltJRgtQjPRQW/o6uVIh5nr+AOmu1vgnf8dD5KKvRnzjncQ43DSDRstDhqIcEhCn9er2i2JtAHeZuhs2ZhBQLDk1gMVUhadaN3pZ/XU8BBbpir4LIW3E378W1GYrLdDdwBwKBgQDJV9FBUW5tIMt7Bxbw76lbmytXaAFvQLMeZZwn1j8wRkqmPGFut0javsPIwgrfP5Qgeu7RlJDd495CCAQu9vIuzdjc6MLiPmz+NmjSS2htKb4hM5C2wsfI1IqqYt/AwXPQuT6bisRq4S1ava7JRwDY+q0NOjoEaIIFJaGFq1O1gwKBgBQA1j+jvIpqy9IaD8vBCPJjiQueleCwkcoL4gM35fsNM9QAGsYnbdFKOM8l+kYNMZCZFrVK8hFTkDZeUVLAGadPIjcsO9O6qQPL04TnQuD/J7BabmfffmEP8+ICpnXPqNjD+XqOglnpIyMBOgwahVpVsEnYT+GbcNC1gwgRErHLAoGAQE0ddSDOhWeN1JKlDvlbOvhJVTbQDnm5OqH0xvwzXfV07bYm37cFO2blG/5sfnPNmLnp/2DVCyg02R26SE1xduUitxpW8u5A3Mb/nvmaNhK4t93B/7whFdBbIKNHFkYx+JzQk9gzdnbHh01AvuNAMAuOrMTFtpaxv3cPKKNYroUCgYArBfveStNE1cLtZEAInagSrmcwowVfYP2TFCtIe0oG5iMNE9jy5b5nYjRRyD/2PJ0VGSCm0ptTP3sIETmMlFWPGydXIVid9ihuLipFfaJVzpO8Uzf1bcU/aboWEm6v9+jcst2zzpBblieoIQcB49/u5uwEdGRmMaUqV3LOZInfyw==-----END PRIVATE KEY-----"sv};

/**
 * @brief authentication_adapter_mock
 * @details This mock does not issue tokens, but verifies that it is signed with the correct private key.
 * And this mock authenticates encrypted_credential whose username/password combination are  admin-test and user-pass.
 */
class authentication_adapter_mock : public authentication_adapter {
  public:
    explicit authentication_adapter_mock(bool enabled) : enabled_(enabled) {
        if (enabled_) {
            rsa_ = std::make_unique<crypto::rsa>(key);
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
