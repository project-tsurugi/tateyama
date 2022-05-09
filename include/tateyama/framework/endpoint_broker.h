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

#include <takatori/util/fail.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/service.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>

namespace tateyama::framework {

using tateyama::api::server::request;
using tateyama::api::server::response;

using takatori::util::fail;

/**
 * @brief endpoint broker
 * @details endpoint broker is a special service that accepts request from endpoints, convert it to service request
 * and dispatch to services layer.
 */
class endpoint_broker : public service {
public:
    static constexpr id_type tag = service_id_endpoint_broker;

    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief tateyama endpoint service interface
     * @param req the request
     * @param res the response
     * @details this function provides API for tateyama AP service (routing requests to server AP and
     * returning response and application output through data channels.)
     * Endpoint uses this function to transfer request to AP and receive its response and output.
     * `request`, `response` and IF objects (such as data_channel) derived from them are expected to be implemented
     * by the Endpoint so that it provides necessary information in request, and receive result or notification
     * in response.
     * This function is asynchronous, that is, it returns as soon as the request is submitted and scheduled.
     * The caller monitors the response and data_channel to check the progress. See tateyama::api::endpoint::response
     * for details. Once request is transferred and fulfilled by the server AP, the response and data_channel
     * objects member functions are called back to transfer the result.
     * @note this function is thread-safe. Multiple client threads sharing this database object can call simultaneously.
     */
    virtual tateyama::status operator()(
        std::shared_ptr<api::endpoint::request const> req,
        std::shared_ptr<api::endpoint::response> res) = 0;

    /**
     * @brief api function for framework::service - this should not be called
     * @details kept for consistency to run as framework::service.
     */
    bool operator()(
        std::shared_ptr<request>,
        std::shared_ptr<response>) override;

};

}

