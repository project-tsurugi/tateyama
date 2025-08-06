/*
 * Copyright 2022-2024 Project Tsurugi.
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
#include <sstream>
#include <array>
#include <stdexcept>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/decoder.h>
#include <openssl/core_names.h>

#include "base64.h"

namespace tateyama::authentication::resource::crypto {

template<typename T, typename D>
std::unique_ptr<T, D> make_handle(T* handle, D deleter)
{
    return std::unique_ptr<T, D>{handle, deleter};
}

class rsa {
public:
    explicit rsa(std::string_view private_key_text) : private_key_(shape_key_string(private_key_text)) {
        evp_private_key_ = make_handle(get_key(private_key_, EVP_PKEY_KEYPAIR), EVP_PKEY_free);
        if (!evp_private_key_) {
            throw std::runtime_error("Get private key failed");
        }
        set_public_key();
    }

    [[nodiscard]] const std::string& public_key() const noexcept {
        return public_key_;
    }
    [[nodiscard]] const std::string& private_key() const noexcept {
        return private_key_;
    }

    void decrypt(std::string_view in, std::string &out) {
        size_t buf_len = 0;
        std::array<OSSL_PARAM, 5> params{};

        auto ctx = make_handle(EVP_PKEY_CTX_new_from_pkey(nullptr, evp_private_key_.get(), nullptr), EVP_PKEY_CTX_free);
        if (!ctx) {
            throw std::runtime_error("fail to EVP_PKEY_CTX_new_from_pkey");
        }

        set_optional_params(params);
        if (EVP_PKEY_decrypt_init_ex(ctx.get(), params.data()) <= 0) {
            throw std::runtime_error("fail to EVP_PKEY_decrypt_init_ex");
        }
        if (EVP_PKEY_decrypt(ctx.get(), nullptr, &buf_len, reinterpret_cast<const unsigned char*>(in.data()), in.length()) <= 0) {  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) due to openssl API
            throw std::runtime_error("fail to EVP_PKEY_decrypt first");
        }
        out.resize(buf_len);
        if (EVP_PKEY_decrypt(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &buf_len, reinterpret_cast<const unsigned char*>(in.data()), in.length()) <= 0) {  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) due to openssl API
            throw std::runtime_error("fail to EVP_PKEY_decrypt final");
        }
        out.resize(buf_len);
    }

private:
    std::string public_key_{};

    std::string private_key_{};
    std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)> evp_private_key_{nullptr, nullptr};

    void set_public_key() {
        auto const out = make_handle(BIO_new(BIO_s_mem()), BIO_free_all);
        if (i2d_PUBKEY_bio(out.get(), evp_private_key_.get()) <= 0) {
            throw std::runtime_error("fail to build public key from private key");
        }

        char* p{};
        auto const length = BIO_get_mem_data(out.get(), &p);
        if (length == -1) {
            throw std::runtime_error("fail to allocate memory with BIO_get_mem_data");
        }
        std::stringstream ssi;
        for (std::int32_t i = 0; i < length; i++) {
            ssi << p[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        std::stringstream sso;
        sso << "-----BEGIN PUBLIC KEY-----\n";
        encode(ssi, sso);
        sso << "\n-----END PUBLIC KEY-----";
        public_key_ = sso.str();
    }

    std::string shape_key_string(const std::string_view key) {
        std::string key_text{key};
        while (true) {
            if (std::size_t pos = key_text.find('\n'); pos != std::string::npos) {
                key_text.replace(pos, 1, "");
            } else {
                break;
            }
        }
        replace(key_text, "-----BEGIN PRIVATE KEY-----", "-----BEGIN PRIVATE KEY-----\n");
        replace(key_text, "-----END PRIVATE KEY-----", "\n-----END PRIVATE KEY-----");
        return key_text;
    }
    void replace(std::string& body, const std::string& origin, const std::string& dest) {
        if (std::size_t pos = body.find(origin); pos != std::string::npos) {
            size_t len = origin.length();
            body.replace(pos, len, dest);
        }
    }

    EVP_PKEY *get_key(const std::string& pem_key_string, int selection) {
        EVP_PKEY *pkey = nullptr;
        const auto *data = reinterpret_cast<const unsigned char*>(pem_key_string.data());  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) due to openssl API
        size_t data_len = pem_key_string.length();

        auto dctx = make_handle(OSSL_DECODER_CTX_new_for_pkey(&pkey, "PEM", nullptr, "RSA",
                                                              selection, nullptr, nullptr), OSSL_DECODER_CTX_free);
        if (OSSL_DECODER_from_data(dctx.get(), &data, &data_len)  <= 0) {
            throw std::runtime_error("fail to handle key");
        }
        return pkey;
    }

    void set_optional_params(std::array<OSSL_PARAM, 5>& params) {
        std::int32_t i = 0;
        params.at(i++) = OSSL_PARAM_construct_utf8_string(const_cast<char *>(OSSL_ASYM_CIPHER_PARAM_PAD_MODE),
                                                          const_cast<char *>(OSSL_PKEY_RSA_PAD_MODE_PKCSV15), 0);  // due to openssl API
        params.at(i) = OSSL_PARAM_construct_end();
    }
};

}  // namespace crypto
