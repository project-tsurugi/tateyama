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

#include <map>
#include <vector>

#include <tateyama/api/server/response.h>

namespace tateyama::loopback {

/**
 * @brief response of loopback endpoint request handling
 * @see buffered_response
 */
class buffered_response {
public:
    /**
     * @brief create the object
     */
    buffered_response() = default;

    /**
     * @brief create response object
     * @param session_id session identifier of the response
     * @param body_head body head of the response
     * @param body body of the response
     * @param data_map all data written to all channels (key: channel name, value: written data)
     */
    buffered_response(std::size_t session_id, std::string_view body_head,
            std::string_view body, std::map<std::string, std::vector<std::string>, std::less<>> &&data_map);

    /**
     * @brief create response object
     * @param session_id session identifier of the response
     * @param error_rec error information record
     */
    buffered_response(std::size_t session_id, proto::diagnostics::Record error_rec);

    /**
     * @brief accessor to the session identifier
     * @return session identifier of this response
     */
    [[nodiscard]] std::size_t session_id() const noexcept;

    /**
     * @brief accessor to the response body head
     * @return body head of this response
     * @return empty string_view object if body head is not defined
     */
    [[nodiscard]] std::string_view body_head() const noexcept;

    /**
     * @brief accessor to the response body
     * @return body of this response
     */
    [[nodiscard]] std::string_view body() const noexcept;

    /**
     * @brief check whether this response has a channel of the specified name
     * @param name a name of the channel
     * @return true if this response has a channel of the specified name
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    [[nodiscard]] bool has_channel(std::string_view name) const noexcept;

    /**
     * @brief retrieve all written data to the channel of the specified name
     * @return all written data to the channel of the specified name
     * @throw std::invalid_argument if name is unknown
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    [[nodiscard]] std::vector<std::string> const& channel(std::string_view name) const;

    /**
     * @brief accessor to the error information
     * @return error information object
     */
    [[nodiscard]] proto::diagnostics::Record const& error() const noexcept;

private:
    std::size_t session_id_ { };
    std::string body_head_ { };
    std::string body_ { };
    std::map<std::string, std::vector<std::string>, std::less<>> data_map_ { };
    proto::diagnostics::Record error_rec_ { };
};

} // namespace tateyama::loopback
