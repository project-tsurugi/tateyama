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

#include <atomic>
#include <memory>
#include <string_view>
#include <exception>

#include <glog/logging.h>
#include <tateyama/logging.h>
#include <tateyama/logging_helper.h>

#include <tateyama/api/server/response.h>
#include "pointer_comp.h"
#include "worker_configuration.h"

namespace tateyama::endpoint::common {
/**
 * @brief response object for common_endpoint
 */
class response : public tateyama::api::server::response {
public:
    response(std::size_t index, const configuration& conf) : index_(index), conf_(conf) {
    }

    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    [[nodiscard]] bool check_cancel() const override {
        return cancel_;
    }

    tateyama::status add_blob(std::unique_ptr<tateyama::api::server::blob_info> blob) override {
        if (!conf_.allow_blob_privileged_) {
            return tateyama::status::operation_denied;
        }
        try {
            blobs_.emplace(std::move(blob));
            return tateyama::status::ok;
        } catch (std::exception &ex) {
            return tateyama::status::unknown;
        }
    }

    void cancel() noexcept {
        VLOG_LP(log_trace) << "set cancel flag for session " << session_id_;
        cancel_ = true;
    }

    [[nodiscard]] bool is_completed() noexcept {
        return completed_ && !data_channel_;
    }

    void set_completed() noexcept {
        completed_ = true;
    }

    [[nodiscard]] constexpr inline std::string_view state_label() noexcept {
        using namespace std::string_view_literals;

        switch (state_) {
        case state::no_data_channel: return "data channel is not used"sv;
        case state::to_be_used: return "data channel is to be used"sv;
        case state::acquired: return "data channel is acquired"sv;
        case state::released: return "data channel is released"sv;
        case state::acquire_failed: return "acqure_channel failed"sv;
        case state::release_failed: return "release_channel failed"sv;
        }
        std::abort();
    }

protected:
    enum class state : std::uint32_t {
        /**
         * @briqef data channel is not used.
         */
        no_data_channel = 0,

        /**
         * @brief response::body_head() has been executed, response::acqure_channel() has not been executed.
         */
        to_be_used,

        /**
         * @brief response::acqure_channel() has been successfully executed.
         */
        acquired = 2,

        /**
         * @brief response::release_channel() has been successfully executed.
         */
        released = 3,

        /**
         * @brief response::acqure_channel() failed.
         */
        acquire_failed = 4,

        /**
         * @brief  response::release_channel() failed.
         */
        release_failed = 5
    };

    std::size_t index_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::size_t session_id_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::shared_ptr<tateyama::api::server::data_channel> data_channel_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::atomic_bool complete_gate_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::set<std::unique_ptr<tateyama::api::server::blob_info>, pointer_comp<tateyama::api::server::blob_info>> blobs_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    void set_state(state s) {
        state_ = s;
    }

private:
    const configuration& conf_;
    bool cancel_{};
    bool completed_{};

    state state_{state::no_data_channel};
};

}  // tateyama::endpoint::common
