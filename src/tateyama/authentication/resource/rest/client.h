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
#include <strings.h>
#include <string>
#include <optional>

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

#include <json-c/json.h>

namespace tateyama::authentication::resource::rest {

class client {
public:
    explicit client(const std::string& url) : connection_(std::make_unique<RestClient::Connection>(url)) {
    }

    std::optional<std::pair<std::string, std::string>> get_encryption_key() {
        auto res = connection_->get("/encryption-key");
        
        std::string type{};
        std::string data{};
        if (parse(res.body, type, "key_type", data, "key_data")) {
            if (!type.empty() && !data.empty()) {
                if (type == "RSA") {
                    return std::make_pair(std::move(type), std::move(data));
                }
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> verify_token(std::string_view token_given) {
        std::string t("Bearer ");
        t += token_given;

        connection_->AppendHeader("Authorization", t);

        auto res = connection_->get("/verify");

        std::string token{};
        if (parse(res.body, token, "token")) {
            if (!token.empty()) {
                return token;
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> verify_encrypted(std::string_view username, std::string_view password) {
        std::string e(username);
        e += ".";
        e += password;
        connection_->AppendHeader("X-Encrypted-Credentials", e);

        auto res = connection_->get("/issue-encrypted");

        std::string token{};
        if (parse(res.body, token, "token")) {
            if (!token.empty()) {
                return token;
            }
        }
        return std::nullopt;
    }

  private:
    std::unique_ptr<RestClient::Connection> connection_;

    bool parse(std::string& body, std::string& p1s, const char p1n[]) {  // NOLINT
        struct json_object *jobj = json_tokener_parse(body.c_str());
        json_object_object_foreach(jobj, key, val) {
            if (strcmp("type", key) == 0) {
                if (strcasecmp("ok", json_object_to_json_string(val)) != 0) {
                    return false;
                }
            }
            if (strcmp(p1n, key) == 0) {
                p1s = json_object_to_json_string(val);
            }
        }
        return true;
    }
    bool parse(std::string& body, std::string& p1s, const char p1n[], std::string& p2s, const char p2n[]) {  // NOLINT
        struct json_object *jobj = json_tokener_parse(body.c_str());
        json_object_object_foreach(jobj, key, val) {
            if (strcmp("type", key) == 0) {
                if (strcasecmp("ok", json_object_to_json_string(val)) != 0) {
                    return false;
                }
            }
            if (strcmp(p1n, key) == 0) {
                p1s = json_object_to_json_string(val);
            }
            if (strcmp(p2n, key) == 0) {
                p2s = json_object_to_json_string(val);
            }
        }
        return true;
    }
};

} // namespace
