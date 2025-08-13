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
#include <memory>
#include <string>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstring>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/decoder.h>
#include <openssl/core_names.h>

#include "base64.h"

namespace tateyama::endpoint::ipc::crypto {

template<typename T, typename D>
std::unique_ptr<T, D> make_handle(T* handle, D deleter)
{
    return std::unique_ptr<T, D>{handle, deleter};
}

class rsa_encrypter {
public:
    explicit rsa_encrypter(std::string_view public_key_text) : public_key_(public_key_text) {
        evp_public_key_ = make_handle(get_public_key(), EVP_PKEY_free);
        if (!evp_public_key_) {
            throw std::runtime_error("Get public key failed.");
        }
        // to avoid "error: ISO C++11 does not allow conversion from string literal to 'char *'"
        std::size_t param_pad_mode_len = strlen(OSSL_ASYM_CIPHER_PARAM_PAD_MODE);
        std::size_t pad_mode_pkcsv15_len = strlen(OSSL_PKEY_RSA_PAD_MODE_PKCSV15);
        // +1 for '\0'
        param_pad_mode_.resize(param_pad_mode_len + 1);
        pad_mode_pkcsv15_.resize(pad_mode_pkcsv15_len + 1);
        // copy string literal to char *
        strcpy(param_pad_mode_.data(), OSSL_ASYM_CIPHER_PARAM_PAD_MODE);
        strcpy(pad_mode_pkcsv15_.data(), OSSL_PKEY_RSA_PAD_MODE_PKCSV15);
    }

    void encrypt(std::string_view in, std::string &out) {
        size_t buf_len = 0;
        OSSL_PARAM params[5];  // NOLINT

        auto ctx = make_handle(EVP_PKEY_CTX_new_from_pkey(nullptr, evp_public_key_.get(), nullptr), EVP_PKEY_CTX_free);
        if (!ctx) {
            throw std::runtime_error("fail to EVP_PKEY_CTX_new_from_pkey");
        }
        set_optional_params(params);  // NOLINT
        if (EVP_PKEY_encrypt_init_ex(ctx.get(), params) <= 0) {  // NOLINT
            throw std::runtime_error("fail to EVP_PKEY_encrypt_init_ex");
        }
        if (EVP_PKEY_encrypt(ctx.get(), nullptr, &buf_len, reinterpret_cast<const unsigned char*>(in.data()), in.length()) <= 0) {  // NOLINT
            throw std::runtime_error("fail to EVP_PKEY_encrypt first");
        }
        out.resize(buf_len);
        if (EVP_PKEY_encrypt(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &buf_len, reinterpret_cast<const unsigned char*>(in.data()), in.length()) <= 0) {  // NOLINT
            throw std::runtime_error("fail to EVP_PKEY_encrypt final");
        }
    }

private:
    std::string public_key_{};

    std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)> evp_public_key_{nullptr, nullptr};

    // to avoid "error: ISO C++11 does not allow conversion from string literal to 'char *'"
    std::vector<char> param_pad_mode_{};
    std::vector<char> pad_mode_pkcsv15_{};

    EVP_PKEY *get_public_key() {
        EVP_PKEY *pkey = nullptr;
        int selection{EVP_PKEY_PUBLIC_KEY};
        const unsigned char *data{};
        size_t data_len{};

        data = reinterpret_cast<const unsigned char*>(public_key_.data());  // NOLINT
        data_len = public_key_.length();
        auto dctx = make_handle(OSSL_DECODER_CTX_new_for_pkey(&pkey, "PEM", nullptr, "RSA",
                                                              selection, nullptr, nullptr), OSSL_DECODER_CTX_free);
        if (OSSL_DECODER_from_data(dctx.get(), &data, &data_len)  <= 0) {
            throw std::runtime_error("fail to handle public key");
        }
        return pkey;
    }

    void set_optional_params(OSSL_PARAM *p) {
        /* "pkcs1" is used by default if the padding mode is not set */
        *p++ = OSSL_PARAM_construct_utf8_string(param_pad_mode_.data(), pad_mode_pkcsv15_.data(), 0);  // NOLINT (same as library usage example)
        *p = OSSL_PARAM_construct_end();
    }
};

}  // namespace tateyama::endpoint::ipc::crypto
