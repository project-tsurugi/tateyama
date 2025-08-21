/*
 * Copyright 2022-2025 Project Tsurugi.
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
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <errno.h>

#include <jwt.h>

namespace tateyama::endpoint::ipc::jwt {

class token_creator {
public:
    token_creator(const std::string& private_key) {
        if (jwt_new(&jwtp_) != 0) {
            return;
        }
        if (jwt_set_alg(jwtp_, JWT_ALG_RS256, reinterpret_cast<const unsigned char*>(private_key.c_str()), private_key.length()) != 0) {
            return;
        }
    }
    ~token_creator() {
        if (token_) {
            free(token_);
        }
        if (jwtp_) {
            jwt_free(jwtp_);
        }
    }
    token_creator(token_creator const& other) = delete;
    token_creator& operator=(token_creator const& other) = delete;
    token_creator(token_creator&& other) noexcept = delete;
    token_creator& operator=(token_creator&& other) noexcept = delete;

    std::optional<std::string> create_token(const std::string& name, long exp) {
        if (jwtp_) {
            if (jwt_add_grant(jwtp_, "tsurugi/auth/name", name.c_str()) != 0) {
                return std::nullopt;
            }
            if (jwt_add_grant_int(jwtp_, "exp", exp) != 0) {
                return std::nullopt;
            }
            if (jwt_add_grant(jwtp_, "sub", "access") != 0) {
                return std::nullopt;
            }
            if (token_) {
                free(token_);
            }
            token_ = jwt_encode_str(jwtp_);
            return token_;
        }
        return std::nullopt;
    }

private:
    jwt_t* jwtp_;
    char* token_{};
};

}  // namespace jwt
