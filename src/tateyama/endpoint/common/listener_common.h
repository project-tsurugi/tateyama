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

#include <ostream>
#include <memory>
#include <chrono>
#include <functional>

#include <tateyama/api/server/request.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/session/resource/bridge.h>
#include <tateyama/status/resource/bridge.h>
#include "tateyama/authentication/resource/bridge.h"
#include "session_info_impl.h"

namespace tateyama::endpoint::common {

class listener_common {
public:
    using callback = std::function<void(const std::shared_ptr<tateyama::api::server::request>&, std::chrono::system_clock::time_point)>;

    /**
     * @brief create empty object
     */
    explicit listener_common(tateyama::framework::environment& env)
        : cfg_(env.configuration()),
          router_(env.service_repository().find<framework::routing_service>()),
          status_(env.resource_repository().find<status_info::resource::bridge>()) {
    }

    /**
     * @brief create empty object for test (use this CTOR in test only)
     */
    explicit listener_common()
        : cfg_(nullptr),
          router_(nullptr),
          status_(nullptr) {
    }

    /**
     * @brief destruct the object
     */
    virtual ~listener_common() = default;

    listener_common(listener_common const& other) = delete;
    listener_common& operator=(listener_common const& other) = delete;
    listener_common(listener_common&& other) noexcept = delete;
    listener_common& operator=(listener_common&& other) noexcept = delete;

    /**
     * @brief body of the process executed by the listener thread
     */
    virtual void operator()() = 0;

    /**
     * @brief notify startup barrier of the listener bocomes ready
     */
    virtual void arrive_and_wait() = 0;

    /**
     * @brief terminate the listener
     */
    virtual void terminate() = 0;

    /**
     * @brief print diagnostics
     * @param os the output stream
     */
    virtual void print_diagnostic(std::ostream& os) = 0;

    /**
     * @brief apply callback function to ongoing request in the workers belonging to the listener
     * @param func the callback function that receives session_id and request
     */
    virtual void foreach_request(const callback& func) = 0;

protected:
    const std::shared_ptr<api::configuration::whole> cfg_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
    const std::shared_ptr<framework::routing_service> router_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
    const std::shared_ptr<status_info::resource::bridge> status_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
};

}  // tateyama::endpoint::common
