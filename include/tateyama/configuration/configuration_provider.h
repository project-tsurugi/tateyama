/*
 * Copyright 2026-2026 Project Tsurugi.
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

#include <tateyama/framework/resource.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/environment.h>

#include <tateyama/api/server/database_info.h>

namespace tateyama::configuration {

class configuration_provider : public ::tateyama::framework::resource {
public:
    /// @brief resource ID of this component.
    static constexpr id_type tag = tateyama::framework::resource_id_configuration;

    /// @brief human readable label of this component.
    static constexpr std::string_view component_label = "configuration";

    /**
     * @brief provides configuration of the database.
     * @return database information
     */
    [[nodiscard]] virtual api::server::database_info const& database_info() const noexcept = 0;
};

}
