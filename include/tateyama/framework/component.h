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

#include <functional>
#include <memory>
#include <type_traits>

#include <tateyama/utils/cache_align.h>
#include <tateyama/framework/component.h>
#include <tateyama/api/environment.h>

namespace tateyama::framework {

using tateyama::api::environment;

class component {
public:
    using id_type = std::uint32_t;

    static constexpr id_type max_system_reserved_id = 255;

    component() = default;

    virtual void setup(environment&) = 0; // -> ready

    virtual void start(environment&) = 0; // -> activated

    virtual void shutdown(environment&) = 0; // -> deactivated

    virtual ~component() = default; // -> disposed

    virtual std::vector<std::string> configuration_sections() = 0;
};

}

