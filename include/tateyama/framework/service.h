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

#include <tateyama/framework/component.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;

class service: public component {
public:

    /**
     * @brief retrieve id
     * @return the component id of this service. Unique among services.
     */
    [[nodiscard]] virtual id_type id() const noexcept = 0;

    /**
     * @brief interface to exchange request and response
     */
    virtual bool operator()(
        std::shared_ptr<request> req,
        std::shared_ptr<response> res) = 0;

};

}

