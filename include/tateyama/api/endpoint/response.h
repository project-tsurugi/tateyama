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
#include "response_code.h"

namespace tateyama::api::endpoint {

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
    virtual ~response() = default;

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
    virtual void code(response_code code) = 0;

    /**
     * @brief notifies the response body head
     * @param body_head the response body head data
     * @details body head provides information for the client to start receiving application output.
     * This function transitions from `initial` to `ready` state, indicating the server sent the information that
     * makes client ready to read application output.
     * @pre body() function of this object is not yet called
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    virtual status body_head(std::string_view body_head) = 0;

    /**
     * @brief notifies the response body
     * @param body the response body data
     * @details body provides information about the final result of the request processing.
     * This function transitions to `completed` state, indicating the response and application output transmission
     * are completed.
     * @pre code() function of this object is already called
     * @pre acquired data channels from this object and acquired writers from those channels are already released
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    virtual status body(std::string_view body) = 0;

    /**
     * @brief retrieve output data channel
     * @param name the name of the output
     * @param ch [out] the data channel for the given name
     * @detail this function provides the named data channel for the application output
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @return status::ok when successful
     * @return other code when error occurs
     */
    virtual status acquire_channel(std::string_view name, data_channel*& ch) = 0;

    /**
     * @brief release the data channel
     * @param ch the data channel to stage
     * mark the data channel staged and return its ownership
     * @details releasing the channel declares finishing using the channel and return it to caller.
     * The caller must not call any of the `ch` member functions any more.
     * @pre acquired writers are already released
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @return status::ok when successful
     * @return other status code when error occurs
     */
    virtual status release_channel(data_channel& ch) = 0;

    /**
     * @brief notify endpoint to close the current session
     * @detail this function is called only in response to `disconnect` message to close the session
     * @warning this function is temporary because endpoint requires the notification to ending session while
     * the notion of session is still evolving. This function can be changed, or completely remove in the future.
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    virtual status close_session() = 0;
};

}
