/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <tateyama/proto/diagnostics.pb.h>

#include "data_channel.h"

namespace tateyama::api::server {

/**
 * @brief response interface
 */
class response {
public:
    ///@brief unknown session id
    constexpr static std::size_t unknown = static_cast<std::size_t>(-1);

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

    /**
     * @brief setter of the session id
     * @param id the session id
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    virtual void session_id(std::size_t id) = 0;

    /**
     * @brief report error with diagnostics information.
     * @param record the diagnostic record to report
     * @details report an error with diagnostics information for client. When this function is called, no more
     * body_head() or body() is expected to be called.
     * @attention this function is not thread-safe and should be called from single thread at a time.
     * @attention After calling this for cancelling the current job, the job must not use the related resources.
     *    This includes the below:
     *
     *    - request object
     *    - response object
     *    - resources underlying session context
     */
    virtual void error(proto::diagnostics::Record const& record) = 0;

    /**
     * @brief setter of the response body head
     * @param body_head the response body head data
     * @pre body() function of this object is not yet called
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    virtual status body_head(std::string_view body_head) = 0;

    /**
     * @brief setter of the response body
     * @param body the response body data
     * @return status::ok when successful
     * @return other code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    virtual status body(std::string_view body) = 0;

    /**
     * @brief retrieve output data channel
     * @param name the name of the output
     * @param ch [out] the data channel for the given name
     * @param writer_count the numver of the writers used in the output
     * @detail this function provides the named data channel for the application output
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @return status::ok when successful
     * @return other code when error occurs
     */
    virtual status acquire_channel(std::string_view name, std::shared_ptr<data_channel>& ch, std::size_t max_writer_count) = 0;

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
    virtual status release_channel(data_channel& ch) = 0;

    /**
     * @brief returns whether or not cancellation was requested to the corresponding job.
     * @details To cancel the job, first you must shutdown the operation of this job, and then call error().
     *    At this time, `OPERATION_CANCELED` is recommended as the diagnostic code for cancelling the job.
     *    Or, to avoid inappropriate conditions, you can omit the cancel request and complete the job.
     * @return true if the job is calling for a cancel
     * @return false otherwise
     * @see error()
     */
    [[nodiscard]] virtual bool check_cancel() const = 0;

};

}
