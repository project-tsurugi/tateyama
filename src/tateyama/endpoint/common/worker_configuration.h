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
#include <tateyama/grpc/blob_relay/service_adapter.h>
#include <tateyama/session/resource/bridge.h>

#include <tateyama/api/configuration.h>
#include <tateyama/configuration/configuration_provider.h>
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
    configuration(connection_type con, tateyama::framework::environment& env)
        : con_(con),
          session_(env.resource_repository().find<tateyama::session::resource::bridge>()),
          database_info_(database_info(env)),
          auth_(authentication_bridge(env)),
          administrators_(env.configuration()),
          blob_relay_service_(tateyama::endpoint::common::configuration::blob_service(env)) {

        if (const auto& cfg = env.configuration(); cfg) {
            // for blob_relay
            if (auto* grpc_server = cfg->get_section("grpc_server"); grpc_server) {
                auto grpc_enabled_opt = grpc_server->get<bool>("enabled");
                auto endpoint_opt = grpc_server->get<std::string>("endpoint");
                auto secure_opt = grpc_server->get<bool>("secure");
                auto* blob_relay = cfg->get_section("blob_relay");
                if (grpc_enabled_opt && endpoint_opt && secure_opt && blob_relay) {
                    auto blob_relay_enabled_opt = blob_relay->get<bool>("enabled");
                    auto stream_chunk_size_opt = blob_relay->get<std::string>("stream_chunk_size");
                    if (blob_relay_enabled_opt) {
                        if (blob_relay_enabled_opt.value()) {
                            blob_relay_enabled_ = true;
                            blob_relay_endpoint_ = endpoint_opt.value();
                            blob_relay_secure_ = secure_opt.value();
                            if (stream_chunk_size_opt) {
                                using namespace std::literals::string_literals;
                                blob_relay_streaming_params_.emplace("stream_chunk_size"s, stream_chunk_size_opt.value());
                            }
                        }
                    }
                }
            }
        }
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

    // for initialization
    static api::server::database_info const& database_info(tateyama::framework::environment& env) {
        if (auto configuration_provider = env.resource_repository().find<tateyama::configuration::configuration_provider>(); configuration_provider) {
            return configuration_provider->database_info();
        }
        throw std::runtime_error("no configuration_provider");
    }
    static std::shared_ptr<tateyama::authentication::resource::bridge> authentication_bridge(tateyama::framework::environment& env) {
        if (auto enabled_opt = env.configuration()->get_section("authentication")->get<bool>("enabled"); enabled_opt) {
            if (enabled_opt.value()) {
                return env.resource_repository().find<tateyama::authentication::resource::bridge>();
            }
        }
        return nullptr;
    }
    static std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_service(tateyama::framework::environment& env) {
        if (auto service_adapter = env.resource_repository().find<tateyama::grpc::blob_relay::service_adapter>(); service_adapter) {
            return service_adapter->blob_relay_service();
        }
        return nullptr;
    }

private:
    const connection_type con_;
    const std::shared_ptr<tateyama::session::resource::bridge> session_;
    const tateyama::api::server::database_info& database_info_;
    const std::shared_ptr<authentication::resource::bridge> auth_;
    const administrators administrators_;
    const std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service_;
    bool enable_timeout_{};
    bool allow_blob_privileged_{};
    std::size_t refresh_timeout_{};
    std::size_t max_refresh_timeout_{};
    std::size_t authentication_timeout_{};

    // for blob_relay
    bool blob_relay_enabled_{};
    std::string blob_relay_endpoint_{};
    bool blob_relay_secure_{};
    std::map<std::string, std::string> blob_relay_streaming_params_{};

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

    resources(const configuration& config, std::size_t session_id, std::string_view conn_info)
      : session_id_(session_id),
        session_info_(session_id_, connection_label(config.con_), conn_info, config.administrators_),
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
    [[nodiscard]] inline blob_transfer_type blob_transfer() const noexcept {
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
