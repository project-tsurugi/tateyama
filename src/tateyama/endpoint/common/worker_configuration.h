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

class configuration {
public:
    configuration(connection_type con, std::shared_ptr<tateyama::session::resource::bridge> session) :
        con_(con), session_(std::move(session)) {
    }
    explicit configuration(connection_type con) : configuration(con, nullptr) {  // for tests
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

private:
    const connection_type con_;
    const std::shared_ptr<tateyama::session::resource::bridge> session_;
    bool enable_timeout_{};
    bool allow_blob_privileged_{};
    std::size_t refresh_timeout_{};
    std::size_t max_refresh_timeout_ {};

    friend class worker_common;
    friend class request;
    friend class response;
};

}  // tateyama::endpoint::common
