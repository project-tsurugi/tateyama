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

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/metrics/resource/bridge.h>

namespace tateyama::metrics::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

extern __thread bool ipc_correction;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * @brief metrics service main object
 */
class core {
public:
    core() = default;

    explicit core(std::shared_ptr<tateyama::api::configuration::whole> cfg);

    bool start(tateyama::metrics::resource::bridge* resource);

    bool shutdown(bool force = false);

    bool operator()(
        const std::shared_ptr<request>& req,
        const std::shared_ptr<response>& res
    );

private:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
    tateyama::metrics::resource::bridge* resource_{};
};

}
