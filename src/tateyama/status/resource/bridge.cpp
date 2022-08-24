/*
 * Copyright 2018-2022 tsurugi project.
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
#include <iostream>  // FIXME
#include <openssl/md5.h>

#include "tateyama/status/resource/bridge.h"

namespace tateyama::status_info::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment& env) {
    set_digest(env.configuration()->get_canonical_path().string());

    std::string status_file_name = (digest_ + ".stat");
    shm_remover_ = std::make_unique<shm_remover>(status_file_name);
    try {
        segment_ = std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, status_file_name.c_str(), shm_size);
        resource_status_memory_ = std::make_unique<resource_status_memory>(*segment_);
        resource_status_memory_->set_pid();
        return true;
    } catch(const boost::interprocess::interprocess_exception& ex) {
        return false;
    }
}

bool bridge::start([[maybe_unused]] environment& env) {
    return true;
}

bool bridge::shutdown([[maybe_unused]] environment&) {
    deactivated_ = true;
    return true;
}

bridge::~bridge() = default;

void bridge::whole(state s) {
    if (resource_status_memory_) {
        resource_status_memory_->whole(s);
    }
}

void bridge::set_digest(const std::string& path_string) {
    MD5_CTX mdContext;
    unsigned int len = path_string.length();
    std::array<unsigned char, MD5_DIGEST_LENGTH> digest{};

    std::string s{};
    s.resize(len + 1);
    path_string.copy(s.data(), len + 1);
    s.at(len) = '\0';
    MD5_Init(&mdContext);
    MD5_Update (&mdContext, s.data(), len);
    MD5_Final(digest.data(), &mdContext);

    digest_.resize(MD5_DIGEST_LENGTH * 2);
    auto it = digest_.begin();
    for(unsigned char c : digest) {
        std::uint32_t n = c & 0xf0U;
        n >>= 8U;
        *(it++) = (n < 0xa) ? ('0' + n) : ('a' + (n - 0xa));
        n = c & 0xfU;
        *(it++) = (n < 0xa) ? ('0' + n) : ('a' + (n - 0xa));
    }
}

} // namespace tateyama::status_info::resource
