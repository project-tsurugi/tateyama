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
#pragma once

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>
#include <tateyama/datastore/resource/bridge.h>
#include <limestone/api/datastore.h>

namespace tateyama::datastore::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

class limestone_backup {
public:
    explicit limestone_backup(limestone::api::backup& backup) : backup_(backup) {}
    /**
     * @brief create empty handle - null reference
     */
    limestone_backup() = delete;

    /**
     * @brief destruct the object
     */
    ~limestone_backup() = default;

    limestone_backup(limestone_backup const& other) = delete;
    limestone_backup& operator=(limestone_backup const& other) = delete;
    limestone_backup(limestone_backup&& other) noexcept = delete;
    limestone_backup& operator=(limestone_backup&& other) noexcept = delete;

    limestone::api::backup& backup() { return backup_; }

private:
    limestone::api::backup& backup_;
};

/**
 * @brief datastore service main object
 */
class core {
public:
    core() = default;

    explicit core(std::shared_ptr<tateyama::api::configuration::whole> cfg);

    bool start(tateyama::datastore::resource::bridge* resource);

    bool shutdown(bool force = false);

    bool operator()(
        const std::shared_ptr<request>& req,
        const std::shared_ptr<response>& res
    );

private:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
    tateyama::datastore::resource::bridge* resource_{};

    std::unique_ptr<limestone_backup> backup_{};
    std::uint64_t backup_id_{};
//    limestone::api::backup_detail backup_detail_;
};

}
