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

#include <csignal>
#include <functional>

#include <sharksfin/api.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/environment.h>

namespace tateyama::diagnostic::resource {

/**
 * @brief diagnostic resource bridge for tateyama framework
 * @details This object bridges diagnostic as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class bridge : public framework::resource {
public:
    static constexpr id_type tag = framework::resource_id_diagnostic;

    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(framework::environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(framework::environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(framework::environment&) override;

    /**
     * @brief add a diagnostic handler
     */
    void add_handler(const std::function<void(std::ostream&)>&);

    /**
     * @brief destructor the object
     */
    ~bridge() override;

    bridge(bridge const& other) = delete;
    bridge& operator=(bridge const& other) = delete;
    bridge(bridge&& other) noexcept = delete;
    bridge& operator=(bridge&& other) noexcept = delete;

    /**
     * @brief create empty object
     */
    bridge() = default;

    /**
     * @brief signal handler calls sharksfin::print_diagnostics(std::ostream&)
     */
    void sighup_handler(int);

private:
    std::vector<std::function<void(std::ostream&)>> handlers_;
};

} // namespace tateyama::diagnostic::resource
