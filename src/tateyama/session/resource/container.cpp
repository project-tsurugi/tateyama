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

#include "tateyama/session/resource/container.h"

namespace tateyama::session::resource {

bool session_container::register_session(std::shared_ptr<session_context> const& session) {
    auto id = session->numeric_id();
    
    session_contexts_.insert(std::make_pair(id, session));
    numeric_ids_.emplace_back(id);
    return true;
}

std::shared_ptr<session_context> session_container::find_session(session_context::numeric_id_type numeric_id) const {
    auto it = session_contexts_.find(numeric_id);

    if (it != session_contexts_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<session_context::numeric_id_type> session_container::enumerate_numeric_ids() const {
    return numeric_ids_;
}

std::vector<session_context::numeric_id_type> session_container::enumerate_numeric_ids(std::string_view symbolic_id) const {
    std::vector<session_context::numeric_id_type> rv{};

    for (const auto& e : session_contexts_) {
        if (symbolic_id == e.second->symbolic_id()) {
            rv.emplace_back(e.second->numeric_id());
        }
    }
    return rv;
}

}
