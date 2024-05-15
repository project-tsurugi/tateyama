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

#include <memory>
#include <functional>

#include <tateyama/session/context.h>

namespace tateyama::endpoint::common {
class worker_common;
}

namespace tateyama::session::resource {

/**
 * @brief Provides living database sessions.
 */
class session_context_impl : public tateyama::session::session_context {
public:
    session_context_impl(session_info& info, session_variable_set variables) noexcept;

    ~session_context_impl() {
        clean_up_();
    }
    
    session_context_impl(session_context_impl const&) = delete;
    session_context_impl(session_context_impl&&) = delete;
    session_context_impl& operator = (session_context_impl const&) = delete;
    session_context_impl& operator = (session_context_impl&&) = delete;

    void set_worker(const std::shared_ptr<tateyama::endpoint::common::worker_common>& worker) {
        session_worker_ = worker;
    }
    std::shared_ptr<tateyama::endpoint::common::worker_common> get_session_worker() {
        return  session_worker_.lock();
    };

private:
    std::function<void(void)> clean_up_{[](){}};
    std::weak_ptr<tateyama::endpoint::common::worker_common> session_worker_{};
};

}
