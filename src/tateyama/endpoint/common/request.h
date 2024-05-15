/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <tateyama/api/server/request.h>

#include "tateyama/status/resource/database_info_impl.h"
#include "session_info_impl.h"

namespace tateyama::endpoint::common {
/**
 * @brief request object for common_endpoint
 */
class request : public tateyama::api::server::request {
public:
    explicit request(const tateyama::api::server::database_info& database_info, const tateyama::api::server::session_info& session_info, tateyama::api::server::session_store& session_store) :
        database_info_(database_info), session_info_(session_info), session_store_(session_store) {
    }

    [[nodiscard]] tateyama::api::server::database_info const& database_info() const noexcept override {
        return database_info_;
    }

    [[nodiscard]] tateyama::api::server::session_info const& session_info() const noexcept override {
        return session_info_;
    }

    [[nodiscard]] tateyama::api::server::session_store& session_store() noexcept override {
        return session_store_;
    }

protected:
    const tateyama::api::server::database_info& database_info_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    const tateyama::api::server::session_info& session_info_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    tateyama::api::server::session_store& session_store_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
};

}  // tateyama::endpoint::common
