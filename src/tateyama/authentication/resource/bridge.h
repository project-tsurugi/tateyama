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
#pragma once

#include <optional>
#include <utility>
#include <string>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/resource.h>

#include <tateyama/proto/auth/response.pb.h>
#include "authentication_adapter.h"

namespace tateyama::authentication::resource {

/**
 * @brief auth resource bridge for tateyama framework
 * @details This object bridges auth as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class bridge : public framework::resource {
    constexpr static std::string_view KEY_TYPE = "RSA";

public:
    static constexpr id_type tag = framework::resource_id_authentication;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "auth_resource";

    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(framework::environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(framework::environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(framework::environment&) override;

    /**
     * @brief destructor the object
     */
    ~bridge() override;

    bridge(bridge const& other) = delete;
    bridge& operator=(bridge const& other) = delete;
    bridge(bridge&& other) noexcept = delete;
    bridge& operator=(bridge&& other) noexcept = delete;

    /**
     * @brief create empty object
     */
    bridge() = default;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

    /**
     * @brief returns a RSA public key for encrypting username and password.
     * @return a RSA public key in pem format
     * @return std::nullopt if the encryption key is not available
     */
    [[nodiscard]] std::optional<std::string> get_encryption_key() const;

    /**
     * @brief verify the authentication token.
     * @param token the token string
     * @return the authenticated username if the token is valid
     * @return the authenticated username and a refresh token if the credentials are valid
     * @throws authentication_exception if the token is malformed
     * @throws authentication_exception if the authentication service is not available
     * @throws authentication_exception if I/O error occurred while verifying the token
     */
    [[nodiscard]] std::optional<std::string> verify_token(std::string_view token) const;

    /**
     * @brief verify the encrypted credentials.
     * @param user_pass the encrypted username and password
     * @return the authenticated username if the token is valid
     * @return std::nullopt if the credential is invalid
     */
    [[nodiscard]] std::optional<std::string> verify_encrypted(std::string user_pass) const;

    /**
     * @brief replace authentication_adapter, provided for test purpose
     * @param adapter the adapter object to replace
     */
    void replace_authentication_adapter(std::unique_ptr<authentication_adapter> adapter) {
        authentication_adapter_ = std::move(adapter);
    }

private:
    std::unique_ptr<authentication_adapter> authentication_adapter_{};
    std::string url_{};
    std::size_t request_timeout_{};
    bool enabled_{};
};

}
