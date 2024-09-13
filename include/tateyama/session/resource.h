/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include <tateyama/session/core.h>

namespace tateyama::session {

/**
 * @brief the core class of `sessions` resource that provides information about living sessions.
 */
class session_resource : public framework::resource {
public:
    static constexpr id_type tag = framework::resource_id_session;

    /**
     * @brief returns sessions_core
     */
    virtual tateyama::session::sessions_core& sessions_core() noexcept = 0;
};

}
