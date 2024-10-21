/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <thread>
#include <chrono>

#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>

#include "tateyama/status/resource/database_info_impl.h"
#include "tateyama/endpoint/common/session_info_impl.h"

namespace tateyama::altimeter {

class test_request : public api::server::request {
public:
    test_request(
        std::size_t session_id,
        std::size_t service_id,
        std::string_view payload
    ) :
        session_id_(session_id),
        service_id_(service_id),
        payload_(payload)
        {}

    [[nodiscard]] std::size_t session_id() const override {
        return session_id_;
    }

    [[nodiscard]] std::size_t service_id() const override {
        return service_id_;
    }

    [[nodiscard]] std::string_view payload() const override {
        return payload_;
    }

    tateyama::api::server::database_info const& database_info() const noexcept override {
        return database_info_;
    }

    tateyama::api::server::session_info const& session_info() const noexcept override {
        return session_info_;
    }

    tateyama::api::server::session_store& session_store() noexcept override {
        return session_store_;
    }

    tateyama::session::session_variable_set& session_variable_set() noexcept override {
        return session_variable_set_;
    }

    std::size_t session_id_{};
    std::size_t service_id_{};
    std::string payload_{};

    tateyama::status_info::resource::database_info_impl database_info_{};
    tateyama::endpoint::common::session_info_impl session_info_{};
    tateyama::api::server::session_store session_store_{};
    tateyama::session::session_variable_set session_variable_set_{};
};

class test_response : public api::server::response {
public:
    test_response() = default;

    void session_id(std::size_t id) override {
        session_id_ = id;
    };
    status body_head(std::string_view body_head) override { return status::ok; }
    status body(std::string_view body) override { body_ = body; body_arrived_ = true; return status::ok; }
    void error(proto::diagnostics::Record const& record) override {}
    status acquire_channel(std::string_view name, std::shared_ptr<api::server::data_channel>& ch) override { return status::ok; }
    status release_channel(api::server::data_channel& ch) override { return status::ok; }
    bool check_cancel() const override { return false; }
    std::string& wait_and_get_body() {
        while (!body_arrived_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return body_;
    }

    std::size_t session_id_{};
    std::string body_{};
private:
    bool body_arrived_{};
};

}
