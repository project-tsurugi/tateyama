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

#include <tateyama/framework/endpoint.h>

#include "buffered_response.h"

namespace tateyama::loopback {

/**
 * @brief loopback endpoint for debug or test of Tateyama server modules
 * @details This class designed for developers to debug or make tests of Tsurugi database.
 * After adding this endpoint to the server, you can send any requests you wanted to debug or so.
 * Every requests are handled by the service specified by code service_id at request().
 * This class doesn't define the format of request payload.
 */
class loopback_client {
public:
    /**
     * @brief create new object
     */
    loopback_client();

    /**
     * @brief destruct new object
     */
    ~loopback_client() = default;

    loopback_client(loopback_client const &other) = delete;
    loopback_client& operator=(loopback_client const &other) = delete;
    loopback_client(loopback_client &&other) noexcept = delete;
    loopback_client& operator=(loopback_client &&other) noexcept = delete;

    /**
     * @brief obtains a loopback endpoint
     * @details Return value can be used for add_endpoint(). You should call that function
     * before setup() or start() of the Tateyama server object.
     * @return loopback endpoint
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @see tateyama::framework::server::add_endpoint()
     * @see tateyama::framework::server::setup()
     * @see tateyama::framework::server::start()
     */
    [[nodiscard]] std::shared_ptr<tateyama::framework::endpoint> endpoint() const noexcept;

    /**
     * @brief handle request by loopback endpoint and receive a response
     * @details Send a request through loopback endpoint.
     * A request is handled by the service of service_id.
     * A response will be returned after handling request operation finished.
     * If service_id is unknown, nothing done, an empty response will be returned.
     * @param session_id session identifier of the request
     * @param service_id service identifier of the request
     * @param payload payload binary data of the request
     * @return response of handling the request
     * @throw std::invalid_argument if service_id is unknown
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention this function is blocked until the operation finished.
     */
    buffered_response request(std::size_t session_id, std::size_t service_id, std::string_view payload);

private:
    std::shared_ptr<tateyama::framework::endpoint> endpoint_;
};

} // namespace tateyama::loopback
