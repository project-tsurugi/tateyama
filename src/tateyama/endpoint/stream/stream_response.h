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

#include <set>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <tateyama/api/server/response.h>

#include "tateyama/endpoint/common/pointer_comp.h"
#include "stream.h"

namespace tateyama::endpoint::stream {

class stream_socket;
class stream_request;
class stream_response;
class stream_data_channel;

/**
 * @brief writer object for stream_endpoint
 */
class alignas(64) stream_writer : public tateyama::api::server::writer {
    friend stream_data_channel;

public:
    explicit stream_writer(std::shared_ptr<stream_socket> stream, std::uint16_t slot, unsigned char writer_id)
        : stream_(std::move(stream)), slot_(slot), writer_id_(writer_id) {}
    tateyama::status write(char const* data, std::size_t length) override;
    tateyama::status commit() override;

private:
    std::shared_ptr<stream_socket> stream_;
    std::uint16_t slot_;
    unsigned char writer_id_;
};

/**
 * @brief data_channel object for stream_endpoint
 */
class alignas(64) stream_data_channel : public tateyama::api::server::data_channel {
public:
    stream_data_channel() = delete;
    explicit stream_data_channel(std::shared_ptr<stream_socket> stream, unsigned int slot)
        : stream_(std::move(stream)), slot_(slot) {}
    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) override;
    tateyama::status release(tateyama::api::server::writer& wrt) override;
    [[nodiscard]] unsigned int get_slot() const { return slot_; }

private:
    std::shared_ptr<stream_socket> stream_;
    std::set<std::shared_ptr<stream_writer>, tateyama::endpoint::common::pointer_comp<stream_writer>> data_writers_{};
    std::mutex mutex_{};
    unsigned int slot_;
    std::atomic_char writer_id_{};
};

/**
 * @brief response object for stream_endpoint
 */
class alignas(64) stream_response : public tateyama::api::server::response {
    friend stream_data_channel;

public:
    stream_response(std::shared_ptr<stream_socket> stream, std::uint16_t index);
    stream_response() = delete;

    tateyama::status body(std::string_view body) override;
    tateyama::status body_head(std::string_view body_head) override;
    void error(proto::diagnostics::Record const& record) override;
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) override;
    tateyama::status release_channel(tateyama::api::server::data_channel& ch) override;

    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    [[nodiscard]] bool check_cancel() const override {
        return false;
    }

private:
    std::shared_ptr<stream_socket> stream_;
    std::uint16_t index_;

    std::string message_{};

    std::shared_ptr<stream_data_channel> data_channel_{};

    std::size_t session_id_{};

    std::atomic_flag completed_{};

    void server_diagnostics(std::string_view diagnostic_record);
};

}
