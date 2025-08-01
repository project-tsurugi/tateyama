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

#include "tateyama/authentication/resource/bridge.h"
#include "session_info_impl.h"

namespace tateyama::endpoint::common {

class listener_common {
public:
    using callback = std::function<void(const std::shared_ptr<tateyama::api::server::request>&, std::chrono::system_clock::time_point)>;

    /**
     * @brief create empty object
     */
    explicit listener_common(const std::string& name) : administrators_(name) {
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
    administrators administrators_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    static std::shared_ptr<tateyama::authentication::resource::bridge> authentication_bridge(tateyama::framework::environment& env) {
        if (auto enabled_opt = env.configuration()->get_section("authentication")->get<bool>("enabled"); enabled_opt) {
            if (enabled_opt.value()) {
                return env.resource_repository().find<tateyama::authentication::resource::bridge>();
            }
        }
        return nullptr;
    }

    static std::string administrator_names(tateyama::framework::environment& env) {
        auto* section = env.configuration()->get_section("authentication");
        if (auto enabled_opt = section->get<bool>("enabled"); enabled_opt) {
            if (enabled_opt.value()) {
                if (auto names_opt = section->get<std::string>("administrators"); names_opt) {
                    return names_opt.value();
                }
                throw std::runtime_error("cannot find authentication.administrators in tsurugi.ini");
            }
            return "*";
        }
        throw std::runtime_error("cannot find authentication.enabled in tsurugi.ini");
    }
};

}  // tateyama::endpoint::common
