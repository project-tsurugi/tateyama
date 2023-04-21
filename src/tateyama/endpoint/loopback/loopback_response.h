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
#include <mutex>

#include <tateyama/api/server/response.h>

#include "loopback_data_writer.h"
#include "loopback_data_channel.h"

namespace tateyama::common::loopback {

class loopback_response: public tateyama::api::server::response {
public:
    /**
     * @see `tateyama::server::response::session_id()`
     */
    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }

    /**
     * @see `tateyama::server::response::code()`
     */
    void code(tateyama::api::server::response_code code) override {
        code_ = code;
    }

    [[nodiscard]] tateyama::api::server::response_code code() const noexcept {
        return code_;
    }

    /**
     * @see `tateyama::server::response::body_head()`
     */
    tateyama::status body_head(std::string_view body_head) override {
        body_head_ = body_head;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body_head() const noexcept {
        return body_head_;
    }

    /**
     * @see `tateyama::server::response::body()`
     */
    tateyama::status body(std::string_view body) override {
        body_ = body;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body() const noexcept {
        return body_;
    }

    [[nodiscard]] bool has_channel(std::string_view name) noexcept;

    /**
     * @see `tateyama::server::response::acquire_channel()`
     * @attention This function fails if {@code name} has been already acquired (even if it has been released).
     */
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel> &ch)
            override;

    /**
     * @see `tateyama::server::response::release_channel()`
     */
    tateyama::status release_channel(tateyama::api::server::data_channel&) override;

    /**
     * @see `tateyama::server::response::close_session()`
     */
    tateyama::status close_session() override {
        return tateyama::status::ok;
    }

    void all_committed_data(std::map<std::string, std::vector<std::string>> &data_map);

private:
    std::size_t session_id_ { };
    tateyama::api::server::response_code code_ { };
    std::string body_head_ { };
    std::string body_ { };

    std::shared_timed_mutex mtx_channel_map_{ };
    /*
     * @brief acquired channel map
     * @details add data_channel when acquired, remove it when it's released
     * @note it's empty if all data channels are released
     * @attention use mtx_channel_map_ to be thread-safe
     */
    std::map<std::string, loopback_data_channel*> acquired_channel_map_ { };

    /*
     * @brief all committed data of all data channels
     * @details add data queue when channel is acquired, not remove it even if it's released.
     * Data queue is filled only when the channel is released
     * @note it's not cleared even if a channel is released
     * @note data queue is reused if same name channel is acquired again
     * @attention use mtx_data_map_ to be thread-safe
     */
    std::map<std::string, std::vector<std::string>> released_data_map_ { };

    /*
     * @brief check whether this response has data of the specified name
     * @return true this response has data channel of the specified name, even if the channel is already released
     * @return false otherwise
     * @attention caller must use mtx_channel_map_ to be thread-safe
     */
    [[nodiscard]] bool has_channel_nolock(std::string_view name) const noexcept {
        return released_data_map_.find(std::string { name }) != released_data_map_.cend();
    }
};

} // namespace tateyama::common::loopback
