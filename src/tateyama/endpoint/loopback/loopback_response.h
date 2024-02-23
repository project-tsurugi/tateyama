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
#include <mutex>

#include <tateyama/api/server/response.h>

#include "loopback_data_writer.h"
#include "loopback_data_channel.h"

namespace tateyama::endpoint::loopback {

class loopback_response: public tateyama::api::server::response {
public:
    /**
     * @see tateyama::server::response::session_id()
     */
    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }

    /**
     * @see tateyama::server::response::body_head()
     */
    tateyama::status body_head(std::string_view body_head) override {
        body_head_ = body_head;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body_head() const noexcept {
        return body_head_;
    }

    /**
     * @see tateyama::server::response::body()
     */
    tateyama::status body(std::string_view body) override {
        body_ = body;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body() const noexcept {
        return body_;
    }

    /**
     * @see tateyama::server::response::error()
     */
    void error(proto::diagnostics::Record const& record) override {
        error_rec_ = record;
    }

    [[nodiscard]] proto::diagnostics::Record const& error() const noexcept {
        return error_rec_;
    }

    /**
     * @see tateyama::server::response::acquire_channel()
     */
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel> &ch)
            override;

    /**
     * @see tateyama::server::response::release_channel()
     */
    tateyama::status release_channel(tateyama::api::server::data_channel &ch) override;

    /**
     * @see tateyama::server::response::check_cancel()
     */
    [[nodiscard]] bool check_cancel() const override {
        return false;
    }

    // just for unit test
    [[nodiscard]] std::map<std::string, std::vector<std::string>, std::less<>> const& all_committed_data() const noexcept {
        return committed_data_map_;
    }

    /**
     * retrieve all committed data of all channels
     * @post loopback_data_writer::commit() of all acquired writers
     * @post loopback_data_channel::release() of all acquired writers
     * @post release_channel() of all acquired channels
     * @note this method is intended to call only once after all writing operations are finished clean
     */
    [[nodiscard]] std::map<std::string, std::vector<std::string>, std::less<>> release_all_committed_data() noexcept;

private:
    std::size_t session_id_ { };
    std::string body_head_ { };
    std::string body_ { };
    proto::diagnostics::Record error_rec_ { };

    std::mutex mtx_channel_map_ { };
    /*
     * @brief acquired channel map
     * @details add data_channel when acquired, remove it when it's released
     * @note it's empty if all data channels are released
     * @attention use mtx_channel_map_ to be thread-safe
     */
    std::map<std::string, std::shared_ptr<tateyama::api::server::data_channel>, std::less<>> channel_map_ { };

    std::mutex mtx_committed_data_map_ { };
    /*
     * @brief all committed data of all data channels
     * Data queue is filled only when the channel is released
     * @attention use mtx_committed_data_map_ to be thread-safe
     * @see notes of all_committed_data()
     */
    std::map<std::string, std::vector<std::string>, std::less<>> committed_data_map_ { };
};

} // namespace tateyama::endpoint::loopback
