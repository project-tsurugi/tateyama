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

#include <cstddef>
#include <string>
#include <string_view>

#include <tateyama/api/server/request.h>

#include "tateyama/configuration/resource/database_info_impl.h"
#include "tateyama/endpoint/common/session_info_impl.h"

namespace tateyama::endpoint::loopback {

/**
 * @brief request object for loopback_endpoint
 */
class loopback_request: public tateyama::api::server::request {
public:
    /**
     * @brief create loopback_request
     * @param session_id session identifier of the request
     * @param service_id service identifier of the request
     * @param payload payload binary data of the request
     */
    loopback_request(std::size_t session_id, std::size_t service_id, std::string_view payload) :
            session_id_(session_id), service_id_(service_id), payload_(payload) {
    }

    /**
     * @brief accessor to session identifier
     */
    [[nodiscard]] std::size_t session_id() const override {
        return session_id_;
    }

    /**
     * @brief accessor to target service identifier
     */
    [[nodiscard]] std::size_t service_id() const override {
        return service_id_;
    }

    /**
     * @brief accessor to local identifier (dummy)
     */
    [[nodiscard]] std::size_t local_id() const override {
        return 0;
    }

    /**
     * @brief accessor to the payload binary data
     * @return the view to the payload
     */
    [[nodiscard]] std::string_view payload() const override {
        return payload_;
    }

    [[nodiscard]] tateyama::api::server::database_info const& database_info() const noexcept override {
        return database_info_;
    }

    [[nodiscard]] tateyama::api::server::session_info const& session_info() const noexcept override {
        return session_info_;
    }

    [[nodiscard]] tateyama::api::server::session_store& session_store() noexcept override {
        return session_store_;
    }

    [[nodiscard]] tateyama::session::session_variable_set& session_variable_set() noexcept override {
        return session_variable_set_;
    }

    [[nodiscard]] bool has_blob(std::string_view) const noexcept override {
        return false;
    }

    [[nodiscard]] tateyama::api::server::blob_info const& get_blob(std::string_view) const override {
        throw std::runtime_error("blob is not supported with loopback endpoint");
    }

private:
    const std::size_t session_id_;
    const std::size_t service_id_;
    const std::string payload_;

    const tateyama::configuration::resource::database_info_impl database_info_{};
    const tateyama::endpoint::common::administrators administrators_{"*"};
    const tateyama::endpoint::common::session_info_impl session_info_{session_id_, "loopback", "", administrators_};
    tateyama::api::server::session_store session_store_{};
    tateyama::session::session_variable_set session_variable_set_{};
};

} // namespace tateyama::endpoint::loopback
