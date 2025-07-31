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

namespace tateyama::authentication::resource {

/**
 * @brief authentication_adapter
 */
class authentication_adapter {
public:
    /**
     * @brief key information type for encrypting username and password.
     */
    using encryption_key_type = std::pair<
        std::string, // key type: "RSA"
        std::string  // key bytes
      >;

    authentication_adapter() = default;
    virtual ~authentication_adapter() = default;

    /**
     * @brief returns whether the authentication feature is enabled.
     * @return true if the authentication feature is enabled
     * @return false if the authentication feature is disabled
     * @attention authentication service may be not available even if this returns true (e.g. due to network error)
     */
    [[nodiscard]] virtual bool is_enabled() const noexcept = 0;

    /**
     * @brief verify the authentication token.
     * @param token the token string
     * @return the authenticated username if the token is valid
     * @return std::nullopt if the credential is invalid
     * @throws authentication_exception if the token is malformed
     * @throws authentication_exception if the authentication service is not available
     * @throws authentication_exception if I/O error occurred while verifying the token
     */
    [[nodiscard]] virtual std::optional<std::string> verify_token(std::string_view token) const = 0;

    /**
     * @brief verify the encrypted credentials.
     * @param encrypted_credential the encrypted credentials
     * @return the authenticated username the credentials are valid
     * @return std::nullopt if the username or the password is invalid
     * @throws authentication_exception if the credentials are malformed
     * @throws authentication_exception if the authentication service is not available
     * @throws authentication_exception if I/O error occurred while verifying the credentials
     */
    [[nodiscard]] virtual std::optional<std::string> verify_encrypted(std::string_view encrypted_credential) const = 0;

    /**
     * @brief returns the key for encrypting username and password.
     * @return the encryption key information
     * @return std::nullopt if the encryption key is not available
     * @throws authentication_exception if the authentication service is not available
     * @throws authentication_exception if I/O error occurred while getting the encryption key
     */
    [[nodiscard]] virtual std::optional<encryption_key_type> get_encryption_key() const = 0;

    authentication_adapter(authentication_adapter const& other) = delete;
    authentication_adapter& operator=(authentication_adapter const& other) = delete;
    authentication_adapter(authentication_adapter&& other) noexcept = delete;
    authentication_adapter& operator=(authentication_adapter&& other) noexcept = delete;
};

/**
 * @brief authentication_exception
 */
class authentication_exception : public std::runtime_error {
public:
    /**
     * @brief create exception object.
     * @param message the error message
     */
    explicit authentication_exception(const std::string& message) : std::runtime_error(message) {
    }
};

}  // namespace tateyama::authentication::resource
