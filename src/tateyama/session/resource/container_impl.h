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
#include <set>
#include <vector>
#include <mutex>
#include <functional>

#include "context_impl.h"
#include <tateyama/session/container.h>

namespace tateyama::session::resource {

struct address_compare {
    bool operator() (const std::weak_ptr<session_context_impl> &lhs, const std::weak_ptr<session_context_impl> &rhs)const {
        return std::addressof(lhs) < std::addressof(rhs);
    }
};

/**
 * @brief provides living database sessions.
 */
class session_container_impl : public tateyama::session::session_container {
public:
    /**
     * @brief construct the object
     */
    session_container_impl() = default;

    /**
     * @brief destruct the object
     */
    virtual ~session_container_impl() = default;
    
    bool register_session(std::shared_ptr<session_context_impl> const& session);

    void foreach(const std::function<void(const std::shared_ptr<session_context>&)>& func) override;

    session_container_impl(session_container_impl const&) = delete;
    session_container_impl(session_container_impl&&) = delete;
    session_container_impl& operator = (session_container_impl const&) = delete;
    session_container_impl& operator = (session_container_impl&&) = delete;

private:
    std::set<std::weak_ptr<session_context_impl>, address_compare> session_contexts_{};

    std::mutex mtx_{};
};

}
