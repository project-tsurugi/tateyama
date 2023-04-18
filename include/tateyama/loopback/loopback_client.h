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
     * @brief returns loopback endpoint
     * Return value can be used for {@code add_endpoint()}. You should call that function
     * before {@code setup()} or {@code start()} of the Tateyama server object.
     * @see tateyama::framework::server::add_endpoint(std::shared_ptr<tateyama::framework::endpoint>)
     * @see tateyama::framework::server::setup()
     * @see tateyama::framework::server::start()
     */
    std::shared_ptr<tateyama::framework::endpoint> endpoint() const noexcept;

    /**
     * @brief handle request by loopback endpoint and receive a response
     * @details send a request through loopback endpoint.
     * A request is handled by the service of {@code service_id}.
     * A response will be returned after handling request operation finished.
     * If {@code service_id} is unknown, nothing done, an empty response will be returned.
     * @param session_id session identifier of the request
     * @param service_id service identifier of the request
     * @param payload payload binary data of the request
     * @attention This function is blocked until the operation finished.
     */
    buffered_response request(std::size_t session_id, std::size_t service_id, std::string_view payload);

    /**
     * @brief handle request by loopback endpoint and receive a response
     * @details send a request through loopback endpoint.
     * A request is handled by the service of {@code service_id}.
     * A response will be returned after handling request operation finished.
     * If {@code service_id} is unknown, nothing done, an empty response will be returned.
     * For better performance, {@code recycle} object is always used as a response.
     * All values in {@code recycle} object is overwritten in this function's call.
     * @param session_id session identifier of the request
     * @param service_id service identifier of the request
     * @param payload payload binary data of the request
     * @param recycle response object to be used
     * @attention This function is blocked until the operation finished.
     */
    buffered_response request(std::size_t session_id, std::size_t service_id, std::string_view payload,
            buffered_response &&recycle);

private:
    std::shared_ptr<tateyama::framework::endpoint> endpoint_;
};

} // namespace tateyama::loopback
