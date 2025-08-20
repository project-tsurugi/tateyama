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

namespace tateyama::authentication::resource::jwt {

class token_handler {
public:
    token_handler(std::string_view token, const std::string& public_key) {
        const std::string token_string(token);

        if (jwt_decode(&jwtp_, token_string.c_str(), reinterpret_cast<const unsigned char*>(public_key.c_str()), static_cast<std::int32_t>(public_key.length())) == 0) {  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) due to jwt API
            return;
        }
        jwtp_ = nullptr;
    }
    ~token_handler() {
        jwt_free(jwtp_);
    }
    token_handler(token_handler const& other) = delete;
    token_handler& operator=(token_handler const& other) = delete;
    token_handler(token_handler&& other) noexcept = delete;
    token_handler& operator=(token_handler&& other) noexcept = delete;

    std::optional<std::string> tsurugi_auth_name() {
        if (jwtp_) {
            auto* name = jwt_get_grant(jwtp_, "tsurugi/auth/name");
            if (name) {
                return std::string(name);
            }
        }
        return std::nullopt;
    }
    std::optional<std::int64_t> expiration_time() {
        if (jwtp_) {
            auto rv = jwt_get_grant_int(jwtp_, "exp");
            if (errno != ENOENT) {
                return rv;
            }
        }
        return std::nullopt;
    }


  private:
    jwt_t *jwtp_;
};

}  // namespace jwt
