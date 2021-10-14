/*
 * Copyright 2018-2021 tsurugi project.
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

#include <tateyama/status.h>
#include <tateyama/api/registry/environment.h>

namespace tateyama::api::endpoint {

/**
 * @brief tateyama endpoint provider interface
 * @details this is the interface that commonly implemented by each endpoint (ipc, rest, etc.)
 * This interface is to add in common endpoint registry.
 */
class provider {
public:
    /**
     * @brief create empty object
     */
    provider() = default;

    /**
     * @brief destruct the object
     */
    virtual ~provider() = default;

    provider(provider const& other) = default;
    provider& operator=(provider const& other) = default;
    provider(provider&& other) noexcept = default;
    provider& operator=(provider&& other) noexcept = default;

    /**
     * @brief initialize endpoint
     * @param context the configuration context to initialize the endpoint
     */
    virtual tateyama::status initialize(registry::environment& env, void* context) = 0;

    /**
     * @brief shutdown endpoint
     */
    virtual tateyama::status shutdown() = 0;
};

}
