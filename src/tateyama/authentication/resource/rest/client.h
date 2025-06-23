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

#include <cpprest/http_client.h>

namespace tateyama::authentication::resource::rest {

class client {
public:
    explicit client(const std::string& loc) {
        utility::string_t address = U(loc);
        web::http::uri uri = web::http::uri(address);
        client_ = std::make_unique<web::http::client::http_client>(web::http::uri_builder(uri).to_uri());
    }

    std::optional<std::pair<std::string, std::string>> get_encryption_key() {
        auto response = client_->request(web::http::methods::GET, web::http::uri_builder(U("/encryption-key")).to_string()).get();
        web::json::value body = response.extract_json(true).get();

        std::string type = body.at("key_type").as_string();
        std::string data = body.at("key_data").as_string();

        if (!type.empty() && !data.empty()) {
            if (type == "RSA") {
                return std::make_pair(type, data);
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> verify_token(std::string_view token_given) {
        std::string t("Bearer ");
        t += token_given;

        web::http::http_request req(web::http::methods::GET);
        req.set_request_uri(web::http::uri_builder(U("/verify")).to_string());
        req.headers().add(U("Authorization"), t);

        auto response = client_->request(req).get();
        web::json::value body = response.extract_json(true).get();

        std::string token = body.at("token").as_string();
        if (!token.empty()) {
            return token;
        }
        return std::nullopt;
    }

    std::optional<std::string> verify_encrypted(std::string_view username, std::string_view password) {
        std::string e(username);
        e += ".";
        e += password;

        web::http::http_request req(web::http::methods::GET);
        req.set_request_uri(web::http::uri_builder(U("/issue-encrypted")).to_string());
        req.headers().add(U("X-Encrypted-Credentials"), e);

        auto response = client_->request(req).get();
        web::json::value body = response.extract_json(true).get();

        std::string token = body.at("token").as_string();
        if (!token.empty()) {
            return token;
        }
        return std::nullopt;
    }

  private:
    std::unique_ptr<web::http::client::http_client> client_{};
};

} // namespace
