/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include "info_handler.h"

namespace tateyama::system::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief system service main object
 */
class system_service_core {
public:
    system_service_core();

    bool operator()(
        const std::shared_ptr<request>& req,
        const std::shared_ptr<response>& res
    );

private:
    info_handler info_handler_{};

    friend class system_service_bridge;
};

}
