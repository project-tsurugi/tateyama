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

#include <glog/logging.h>
#include <tateyama/logging.h>
#include <tateyama/logging_helper.h>

#include <tateyama/api/server/response.h>

namespace tateyama::endpoint::common {
/**
 * @brief response object for common_endpoint
 */
class response : public tateyama::api::server::response {
public:
    explicit response(std::size_t index) : index_(index) {
    }

    [[nodiscard]] bool check_cancel() const override {
        return cancel_;
    }

    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    void cancel() noexcept {
        VLOG_LP(log_trace) << "set cancel flag for session " << session_id_;
        cancel_ = true;
        if (data_channel_) {
            release_channel(*data_channel_);
        }
    }

    [[nodiscard]] bool is_completed() noexcept {
        return completed_.load();
    }

    void set_completed() noexcept {
        completed_.store(true);
    }

protected:
    std::size_t index_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::size_t session_id_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::shared_ptr<tateyama::api::server::data_channel> data_channel_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    std::atomic_bool completed_{};  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

private:
    bool cancel_{};
};

}  // tateyama::endpoint::common
