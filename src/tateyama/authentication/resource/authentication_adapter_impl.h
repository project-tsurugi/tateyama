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
#include <memory>
#include <map>
#include <cstdint>
#include <stdexcept>
#include <chrono>

#include "rest/client.h"
#include "authentication_adapter.h"
#include "jwt/token_handler.h"

namespace tateyama::authentication::resource {

/**
 * @brief authentication_adapter_impl
 * @details This mock does not issue tokens, but verifies that it is signed with the correct private key.
 * And this mock authenticates encrypted_credential whose username/password combination are  admin-test and user-pass.
 */
class authentication_adapter_impl : public authentication_adapter {
  public:
    explicit authentication_adapter_impl(bool enabled, const std::string& url_string, std::chrono::milliseconds request_timeout) : enabled_(enabled) {
        if (enabled_) {
            url_parser url(url_string);

            std::string& port = url.port;
            client_ = std::make_unique<rest::client>(url.domain, port.empty() ? 80 : stoi(port), url.path, request_timeout);
            if (encryption_key_ = client_->get_encryption_key(); !encryption_key_) {
                throw std::runtime_error(std::string("cannot get encryption_key from ") + url_string);
            }
        }
    }

    [[nodiscard]] bool is_enabled() const noexcept override {
        return enabled_;
    }

    [[nodiscard]] std::optional<encryption_key_type> get_encryption_key() const override {
        return encryption_key_;
    }

    [[nodiscard]] std::optional<std::string> verify_token(std::string_view token) const override {
        if (enabled_) {
            if (auto token_opt = client_->verify_token(token); token_opt) {
                if (encryption_key_) {
                    auto handler = std::make_unique<jwt::token_handler>(token, encryption_key_.value().second);
                    auto ns = std::chrono::system_clock::now().time_since_epoch();
                    if (std::chrono::duration_cast<std::chrono::seconds>(ns).count() < handler->expiration_time()) {
                        return check_username(handler->tsurugi_auth_name());
                    }
                    switch (handler->token_type()) {
                    case jwt::token_type::access:
                        throw authentication_exception("access token already expired", tateyama::proto::diagnostics::Code::ACCESS_EXPIRED);
                    case jwt::token_type::refresh:
                        throw authentication_exception("refresh token already expired", tateyama::proto::diagnostics::Code::REFRESH_EXPIRED);
                    default:
                        throw authentication_exception("token already expired");
                    }
                }
            }
            throw authentication_exception("verify failed");
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> verify_encrypted(std::string_view encrypted_credential) const override {
        if (enabled_) {
            if (auto token_opt = client_->verify_encrypted(encrypted_credential); token_opt) {
                if (encryption_key_) {
                    auto handler = std::make_unique<jwt::token_handler>(token_opt.value(), encryption_key_.value().second);
                    return check_username(handler->tsurugi_auth_name());
                }
            }
        }
        return std::nullopt;
    }

private:
    bool enabled_;
    std::unique_ptr<rest::client> client_;
    std::optional<std::pair<std::string, std::string>> encryption_key_{};

    class url_parser {
    public:
        explicit url_parser(const std::string& url) {
            boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
            boost::cmatch what;

            if(regex_match(url.c_str(), what, ex)) {
                protocol = std::string(what[1].first, what[1].second);
                domain   = std::string(what[2].first, what[2].second);
                port     = std::string(what[3].first, what[3].second);
                path     = std::string(what[4].first, what[4].second);
                query    = std::string(what[5].first, what[5].second);
            }
        }

    private:
        std::string protocol{};
        std::string domain{};
        std::string port{};
        std::string path{};
        std::string query{};

        friend class authentication_adapter_impl;
    };
};

}
