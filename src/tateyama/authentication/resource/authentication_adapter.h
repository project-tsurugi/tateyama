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

#include <boost/regex.hpp>

#include <tateyama/proto/diagnostics.pb.h>

namespace tateyama::authentication::resource {

/**
 * @brief authentication_exception
 */
class authentication_exception : public std::runtime_error {
public:
    /**
     * @brief create exception object.
     * @param message the error message
     */
    explicit authentication_exception(const std::string& message) : std::runtime_error(message), code_(std::nullopt) {
    }

    /**
     * @brief create exception object.
     * @param message the error message
     * @param code the CoreServiceCode
     */
    authentication_exception(const std::string& message, const tateyama::proto::diagnostics::Code code) : std::runtime_error(message), code_(code) {
    }

    /**
     * @brief returns diagnostics code.
     * @return tateyama::proto::diagnostics::Code
     */
    std::optional<tateyama::proto::diagnostics::Code> code() {
        return code_;
    }

private:
    std::optional<tateyama::proto::diagnostics::Code> code_;
};


/**
 * @brief authentication_adapter
 */
class authentication_adapter {
public:
    /**
     * @brief maximum length for valid username
     */
    constexpr static std::size_t MAXIMUM_USERNAME_LENGTH = 1024;

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

protected:
    static std::optional<std::string> check_username(const std::optional<std::string>& name_opt) {
        if (!name_opt) {
            return std::nullopt;
        }
        const std::string& name = name_opt.value();

        if(regex_match(name, boost::regex(R"(\s.*)"))) {
            throw authentication_exception("invalid user name (begin with whitespace)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
        }
        if(regex_match(name, boost::regex(R"(.*\s)"))) {
            throw authentication_exception("invalid user name (end with whitespace)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
        }
        if (name.length() > MAXIMUM_USERNAME_LENGTH) {
            throw authentication_exception("invalid user name (too long)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
        }
        if(regex_match(name, boost::regex(R"([\x20-\x7E\x80-\xFF]*)"))) {
            return name_opt;
        }
        throw authentication_exception("invalid user name (includes invalid charactor)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);    }
};

}  // namespace tateyama::authentication::resource
