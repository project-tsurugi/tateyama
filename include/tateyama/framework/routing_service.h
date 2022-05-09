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

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief the routing service to dispatch the requests to appropriate service
 */
class routing_service : public service {
public:
    static constexpr id_type tag = service_id_routing;

    [[nodiscard]] id_type id() const noexcept override;

    bool setup(environment& env) override;

    bool start(environment&) override;

    bool shutdown(environment&) override;

    bool operator()(
        std::shared_ptr<request> req,
        std::shared_ptr<response> res) override;

private:
    repository<service>* services_{};
};

}

