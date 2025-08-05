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
#include <string>
#include <string_view>
#include <optional>

#include <boost/regex.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace tateyama::authentication::resource::rest {

class client {
public:
    client(const std::string& host, int port, const std::string& path) :
        client_(std::make_unique<httplib::Client>(host, port)),
        path_(path) {
    }

    std::optional<std::pair<std::string, std::string>> get_encryption_key() {
        auto response = client_->Get((path_ + "/encryption-key").c_str());
        if (response && response->status == 200) {
            nlohmann::json j = nlohmann::json::parse(response->body);

            std::string type = j.find("key_type").value();
            std::string data = j.find("key_data").value();

            if (!type.empty() && !data.empty()) {
                if (type == "RSA") {
                    return std::make_pair(type, data);
                }
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> verify_token(std::string_view token_given) {
        std::string t("Bearer ");
        t += token_given;

        httplib::Headers headers = {
            { "Authorization", t.c_str() }
        };
        auto response = client_->Get((path_ + "/verify").c_str(), headers);
        if (response && response->status == 200) {
            nlohmann::json j = nlohmann::json::parse(response->body);

            std::string token = j.find("token").value();
            if (!token.empty()) {
                return token;
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> verify_encrypted(std::string_view encrypted_credential) {
        std::string ec{encrypted_credential};
        httplib::Headers headers = {
            { "X-Encrypted-Credentials", ec.c_str() }
        };

        auto response = client_->Get((path_ + "/issue-encrypted").c_str(), headers);
        if (response && response->status == 200) {
            nlohmann::json j = nlohmann::json::parse(response->body);

            std::string token = j.find("token").value();
            if (!token.empty()) {
                return token;
            }
        }
        return std::nullopt;
    }

private:
    std::unique_ptr<httplib::Client> client_{};
    std::string path_{};
};

} // namespace
