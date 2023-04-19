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

#include <map>
#include <vector>

#include <tateyama/api/server/response.h>

namespace tateyama::loopback {

/**
 * @brief response of loopback endpoint
 */
class buffered_response {
public:
    /**
     * @brief create response object
     */
    buffered_response() = default;

    /**
     * @brief destruct the object
     */
    ~buffered_response() = default;

    buffered_response(buffered_response const &other) = default;
    buffered_response& operator=(buffered_response const &other) = default;
    buffered_response(buffered_response &&other) noexcept = default;
    buffered_response& operator=(buffered_response &&other) noexcept = default;

    /**
     * @brief update all values in this response
     * @note this function is not thread-safe
     * @note this function is intended to call from internal server side, not to call from client side.
     */
    void update(std::size_t session_id, tateyama::api::server::response_code code, std::string_view body_head,
            std::string_view body, std::map<std::string, std::vector<std::string>> data_map);

    /**
     * @brief accessor to the session identifier
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    std::size_t session_id() const noexcept;

    /**
     * @brief accessor to the tateyama response status
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    tateyama::api::server::response_code code() const noexcept;

    /**
     * @brief accessor to the response body head
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    std::string_view body_head() const noexcept;

    /**
     * @brief accessor to the response body
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    std::string_view body() const noexcept;

    /**
     * @brief returns true if this response has a channel of specified name
     * @param name a name of the channel
     * @return true if this response has a channel of specified name
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    bool has_channel(std::string_view name) const noexcept;

    /**
     * @brief returns a {@code std::vector} of written data to the channel of the specified name
     * @returns every written data to the channel of the specified name
     * @throw out_of_range if this response doesn't have the channel of the specified name
     */
    std::vector<std::string> channel(std::string_view name) const;

private:
    std::size_t session_id_ { };
    tateyama::api::server::response_code code_ { };
    std::string body_head_ { };
    std::string body_ { };
    std::map<std::string, std::vector<std::string>> data_map_ { };
};

} // namespace tateyama::loopback
