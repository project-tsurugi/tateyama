/*
 * Copyright 2018-2021 tsurugi project.
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

#include "data_channel.h"

namespace tateyama::api::endpoint {
class response;
enum class response_code : std::int64_t;
}
namespace tateyama::api::server {

using response_code = tateyama::api::endpoint::response_code;

/**
 * @brief response interface
 */
class response {
public:
    /**
     * @brief create empty object
     */
    response() = default;

    /**
     * @brief destruct the object
     */
    ~response() = default;

    response(response const& other) = default;
    response& operator=(response const& other) = default;
    response(response&& other) noexcept = default;
    response& operator=(response&& other) noexcept = default;

    //    virtual void session_id(std::size_t session) = 0;
    //    virtual void requester_id(std::size_t id) = 0;

    /**
     * @brief setter of the tateyama response status
     * @param st the status code of the response
     * @details This is the status code on the tateyama layer. If application error occurs, the details are stored in
     * the body.
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    void code(response_code code);

    /**
     * @brief setter of the response body head
     * @param body_head the response body head data
     * @pre body() function of this object is not yet called
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    status body_head(std::string_view body_head);

    /**
     * @brief setter of the response body
     * @param body the response body data
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    status body(std::string_view body);

    /**
     * @brief retrieve output data channel
     * @param name the name of the output
     * @param ch [out] the data channel for the given name
     * @detail this function provides the named data channel for the application output
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @return status::ok when successful
     * @return other code when error occurs
     */
    status acquire_channel(std::string_view name, std::shared_ptr<data_channel>& ch);

    /**
     * @brief release the data channel
     * @param ch the data channel to stage
     * mark the data channel staged and return its ownership
     * @details releasing the channel declares finishing using the channel and transfer the channel together with its
     * writers. This function automatically calls data_channel::release() for all the writers that belong to this channel.
     * Uncommitted data on each writer can possibly be discarded. To make release writers gracefully, it's recommended
     * to call data_channel::release() for each writer rather than releasing in bulk with this function.
     * The caller must not call any of the `ch` member functions any more.
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @return status::ok when successful
     * @return other status code when error occurs
     */
    status release_channel(data_channel& ch);

    /**
     * @brief notify endpoint to close the current session
     * @detail this function is called only in response to `disconnect` message to close the session
     * @warning this function is temporary because endpoint requires the notification to ending session while
     * the notion of session is still evolving. This function can be changed, or completely remove in the future.
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    status close_session();

    /**
     * @brief accessor to the endpoint response
     * @return the endpoint response
     */
    [[nodiscard]] std::shared_ptr<api::endpoint::response> const& origin() const noexcept;

private:
    std::shared_ptr<api::endpoint::response> origin_{};
};

}
