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

#include <data_relay_grpc/blob_relay/service.h>
#include <data_relay_grpc/common/session.h>
#include <tateyama/session/resource/bridge.h>

#include <tateyama/api/configuration.h>
#include <tateyama/api/server/session_store.h>
#include <tateyama/session/variable_set.h>
#include <tateyama/authentication/resource/bridge.h>

#include "session_info_impl.h"

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
    configuration(connection_type con, std::shared_ptr<tateyama::session::resource::bridge> session, const tateyama::api::server::database_info& database_info, std::shared_ptr<authentication::resource::bridge> auth, const tateyama::endpoint::common::administrators& administrators, std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service, const std::shared_ptr<api::configuration::whole>& cfg) :
      con_(con), session_(std::move(session)), database_info_(database_info), auth_(std::move(auth)), administrators_(administrators), blob_relay_service_(std::move(blob_relay_service)), cfg_(cfg) {
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
    void set_authentication_timeout(std::size_t timeout) {
        authentication_timeout_ = timeout;
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
    const std::shared_ptr<authentication::resource::bridge> auth_;
    const tateyama::endpoint::common::administrators& administrators_;
    const std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service_;
    const std::shared_ptr<api::configuration::whole> cfg_;
    bool enable_timeout_{};
    bool allow_blob_privileged_{};
    std::size_t refresh_timeout_{};
    std::size_t max_refresh_timeout_{};
    std::size_t authentication_timeout_{};

    [[nodiscard]] std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service() const {
        return blob_relay_service_;
    }
    [[nodiscard]] std::shared_ptr<api::configuration::whole> cfg() const {
        return cfg_;
    }

    friend class worker_common;
    friend class request;
    friend class response;
    friend class resources;
};


class resources {
public:
    class blob_session_container {
    public:
        explicit blob_session_container(data_relay_grpc::common::blob_session& blob_session) : blob_session_(blob_session) {
        }
        [[nodiscard]] data_relay_grpc::common::blob_session& blob_session() const {
            return blob_session_;
        }
    private:
        data_relay_grpc::common::blob_session& blob_session_;
    };

    enum class blob_transfer_type {
        does_not_use = 0,
        privileged = 1,
        blob_relay_streaming = 2,
    };

    resources(const configuration& config, std::size_t session_id, std::string_view conn_info, const tateyama::endpoint::common::administrators& administrators)
      : session_id_(session_id),
        session_info_(session_id_, connection_label(config.con_), conn_info, administrators),
        session_variable_set_(config.session_ ? config.session_->sessions_core().variable_declarations().make_variable_set() : tateyama::session::session_variable_set{}) {
    }

    [[nodiscard]] inline std::size_t session_id() const noexcept {
        return session_id_;
    }
    [[nodiscard]] inline session_info_impl& session_info() noexcept {
        return session_info_;
    }
    [[nodiscard]] inline tateyama::api::server::session_store& session_store() noexcept {
        return session_store_;
    }
    [[nodiscard]] inline tateyama::session::session_variable_set& session_variable_set() noexcept {
        return session_variable_set_;
    }
    inline void blob_session(data_relay_grpc::common::blob_session& blob_session_created) {
        blob_session_container_ = std::make_unique<blob_session_container>(blob_session_created);
    }
    [[nodiscard]] inline data_relay_grpc::common::blob_session& blob_session() const {
        return blob_session_container_->blob_session();
    }
    inline void blob_transfer(blob_transfer_type type) {
        blob_transfer_type_ = type;
    }
    [[nodiscard]] inline blob_transfer_type blob_transfer() const {
        return blob_transfer_type_;
    }

private:
    // session id
    const std::size_t session_id_;

    // session info
    session_info_impl session_info_;

    // session variable set
    tateyama::session::session_variable_set session_variable_set_;

    // session store
    tateyama::api::server::session_store session_store_{};

    // blob_session_container
    std::unique_ptr<blob_session_container> blob_session_container_{};

    // blob_transfer_type
    blob_transfer_type blob_transfer_type_{};

    static std::string_view connection_label(connection_type con) {
        switch (con) {
        case connection_type::ipc:
            return "ipc";
        case connection_type::stream:
            return "tcp";
        default:
            return "";
        }
    }
};

}  // tateyama::endpoint::common
