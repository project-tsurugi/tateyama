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

#include "tateyama/session/resource/container_impl.h"

namespace tateyama::session::resource {

bool session_container_impl::register_session(std::shared_ptr<session_context_impl> const& session) {
    std::unique_lock lock{mtx_};

    for (auto&& it = session_contexts_.begin(), last = session_contexts_.end(); it != last;) {
        if (auto sp = (*it).lock(); sp) {
            ++it;
        } else {
            it = session_contexts_.erase(it);
        }
    }
    if (session_contexts_.find(session) != session_contexts_.end()) {
        return false;
    }
    session_contexts_.emplace(session);
    return true;
}

void session_container_impl::foreach(const std::function<void(const std::shared_ptr<session_context>&)>& func) {
    std::unique_lock lock{mtx_};

    for (auto&& it = session_contexts_.begin(), last = session_contexts_.end(); it != last;) {
        if (auto sp = (*it).lock(); sp) {
            func(sp);
            ++it;
        } else {
            it = session_contexts_.erase(it);
        }
    }
}

}
