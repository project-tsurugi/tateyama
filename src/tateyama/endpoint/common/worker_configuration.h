/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <memory>

#include <tateyama/session/resource/bridge.h>

#include <tateyama/api/server/session_info.h>
#include <tateyama/api/server/session_store.h>
#include <tateyama/session/variable_set.h>

namespace tateyama::endpoint::common {

enum class connection_type : std::uint32_t {
    /**
     * @brief undefined type.
     */
    undefined = 0U,

    /**
     * @brief IPC connection.
     */
    ipc,

    /**
     * @brief stream (TCP/IP) connection.
     */
    stream,
};

class alignas(64) configuration {
public:
    configuration(connection_type con, std::shared_ptr<tateyama::session::resource::bridge> session, const tateyama::api::server::database_info& database_info) :
        con_(con), session_(std::move(session)), database_info_(database_info) {
    }
    explicit configuration(connection_type con, tateyama::api::server::database_info& database_info) : configuration(con, nullptr, database_info) {  // for tests
    }
    void set_timeout(std::size_t refresh_timeout, std::size_t max_refresh_timeout) {
        if (refresh_timeout < 120) {
            throw std::runtime_error("section.refresh_timeout should be greater than or equal to 120");
        }
        if (max_refresh_timeout < 120) {
            throw std::runtime_error("section.max_refresh_timeout should be greater than or equal to 120");
        }
        enable_timeout_ = true;
        refresh_timeout_ = refresh_timeout;
        max_refresh_timeout_ = max_refresh_timeout;
    }
    void allow_blob_privileged(bool allow) {
        allow_blob_privileged_ = allow;
    }
    [[nodiscard]] tateyama::api::server::database_info const& database_info() const noexcept {
        return database_info_;
    }

private:
    const connection_type con_;
    const std::shared_ptr<tateyama::session::resource::bridge> session_;
    const tateyama::api::server::database_info& database_info_;
    bool enable_timeout_{};
    bool allow_blob_privileged_{};
    std::size_t refresh_timeout_{};
    std::size_t max_refresh_timeout_ {};

    friend class worker_common;
    friend class request;
    friend class response;
};

class alignas(64) resources {
public:
  resources(std::size_t session_id,
            const tateyama::api::server::session_info& session_info,
            tateyama::api::server::session_store& session_store,
            tateyama::session::session_variable_set& session_variable_set)
      : session_id_(session_id),
        session_info_(session_info),
        session_store_(session_store),
        session_variable_set_(session_variable_set) {
    }

    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }
    [[nodiscard]] const tateyama::api::server::session_info& session_info() const noexcept {
        return session_info_;
    }
    [[nodiscard]] tateyama::api::server::session_store& session_store() const noexcept {
        return session_store_;
    }
    [[nodiscard]] tateyama::session::session_variable_set& session_variable_set() const noexcept {
        return session_variable_set_;
    }

private:
    std::size_t session_id_;
    const tateyama::api::server::session_info& session_info_;
    tateyama::api::server::session_store& session_store_;
    tateyama::session::session_variable_set& session_variable_set_;
};

}  // tateyama::endpoint::common
