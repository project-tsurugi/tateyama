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
#include <string>
#include <string_view>
#include <optional>
#include <chrono>

#include <boost/regex.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "tateyama/authentication/resource/authentication_adapter.h"  // for authentication_exception

namespace tateyama::authentication::resource::rest {

class client {
public:
    client(const std::string& host, int port, std::string path, std::chrono::milliseconds request_timeout) :
        client_(std::make_unique<httplib::Client>(host, port)),
        path_(std::move(path)) {
        if (request_timeout.count() != 0) {
            client_->set_connection_timeout(request_timeout);
            client_->set_read_timeout(request_timeout);
        }
    }

    std::optional<std::pair<std::string, std::string>> get_encryption_key() {
        auto response = client_->Get((path_ + "/encryption-key"));
        if (response && response->status == 200) {
            nlohmann::json j = nlohmann::json::parse(response->body);

            std::string type = j.find("key_type").value();
            std::string data = j.find("key_data").value();

            if (!type.empty() && !data.empty()) {
                if (type == "RSA") {
                    return std::make_pair(type, data);
                }
            }
            throw authentication_exception("the authentication service malfunction");
        }
        throw authentication_exception("cannot obtain encryption key from the authentication service due to timeout or service unavailable");
    }

    std::optional<std::string> verify_token(std::string_view token_given) {
        std::string t("Bearer ");
        t += token_given;

        httplib::Headers headers = {
            { "Authorization", t.c_str() }
        };
        auto response = client_->Get((path_ + "/verify"), headers);
        if (response) {
            if (response->status == 200) {
                nlohmann::json j = nlohmann::json::parse(response->body);

                std::string token = j.find("token").value();
                if (!token.empty()) {
                    return token;
                }
                throw authentication_exception("the authentication service malfunction");
            }
            throw authentication_exception("the token is malformed");
        }
        throw authentication_exception("cannot verify token by the authentication service due to timeout or service unavailable");
    }

    std::optional<std::string> verify_encrypted(std::string_view encrypted_credential) {
        std::string ec{encrypted_credential};
        httplib::Headers headers = {
            { "X-Encrypted-Credentials", ec.c_str() }
        };

        auto response = client_->Get((path_ + "/issue-encrypted").c_str(), headers);  // NOLINT libcpp-httplib-dev 0.10.3 does not privide Get(std::string) API
        if (response) {
            if (response->status == 200) {
                nlohmann::json j = nlohmann::json::parse(response->body);

                std::string token = j.find("token").value();
                if (!token.empty()) {
                    return token;
                }
                throw authentication_exception("the authentication service malfunction");
            }
            throw authentication_exception("the credential is malformed");
        }
        throw authentication_exception("cannot verify encrypted credential by the authentication service due to timeout or service unavailable");
    }

private:
    std::unique_ptr<httplib::Client> client_;
    std::string path_;
};

} // namespace
