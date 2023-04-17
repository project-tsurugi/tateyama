/*
 * Copyright 2019-2023 tsurugi project.
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

#include <tateyama/framework/environment.h>
#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/status/resource/bridge.h>

#include "tateyama/endpoint/loopback/loopback_request.h"
#include "tateyama/endpoint/loopback/loopback_response.h"

namespace tateyama::framework {

class loopback_endpoint: public endpoint {
public:
    static constexpr std::string_view component_label = "loopback_endpoint";

    bool start(tateyama::framework::environment&) override {
        return true;
    }

    bool setup(tateyama::framework::environment &env) override {
        service_ = env.service_repository().find<tateyama::framework::routing_service>();
        return (service_ != nullptr);
    }

    [[nodiscard]] std::string_view label() const noexcept override {
        return component_label;
    }

    bool shutdown(tateyama::framework::environment&) override {
        return true;
    }

    tateyama::common::loopback::loopback_response request(std::size_t session_id, std::size_t service_id,
            std::string_view payload) {
        auto request = std::make_shared<tateyama::common::loopback::loopback_request>(session_id, service_id, payload);
        auto response = std::make_shared<tateyama::common::loopback::loopback_response>();

        // NOTE: ignore operator() failure
        service_->operator ()(static_cast<std::shared_ptr<tateyama::api::server::request>>(request),
                static_cast<std::shared_ptr<tateyama::api::server::response>>(response));
        return std::move(*response);
    }

private:
    std::shared_ptr<tateyama::framework::routing_service> service_ { };
};

} // namespace tateyama::framework
