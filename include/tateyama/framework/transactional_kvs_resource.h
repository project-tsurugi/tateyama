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

#include <tateyama/framework/ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::framework {

/**
 * @brief resource component
 */
class transactional_kvs_resource : public resource {
public:
    static constexpr id_type tag = resource_id_transactional_kvs;

    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    void setup(environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    void start(environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    void shutdown(environment&) override;
};

}

