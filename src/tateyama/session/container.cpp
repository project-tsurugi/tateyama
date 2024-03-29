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

#include "tateyama/session/container.h"

namespace tateyama::session {

std::shared_ptr<session_context> session_container::find_session(session_context::numeric_id_type numeric_id) const {
    std::shared_ptr<session_context> rv{nullptr};
    const_cast<session_container*>(this)->foreach([&rv, numeric_id](const std::shared_ptr<session_context>& entry) {
        if (numeric_id == entry->numeric_id()) {
            rv = entry;
        }
    });
    return rv;
}

std::vector<session_context::numeric_id_type> session_container::enumerate_numeric_ids() const {
    std::vector<session_context::numeric_id_type> rv{};
    const_cast<session_container*>(this)->foreach([&rv](const std::shared_ptr<session_context>& entry) {
        rv.emplace_back(entry->numeric_id());
    });
    return rv;
}

std::vector<session_context::numeric_id_type> session_container::enumerate_numeric_ids(std::string_view symbolic_id) const {
    std::vector<session_context::numeric_id_type> rv{};
    const_cast<session_container*>(this)->foreach([&rv, symbolic_id](const std::shared_ptr<session_context>& entry) {
        if (symbolic_id == entry->symbolic_id()) {
            rv.emplace_back(entry->numeric_id());
        }
    });
    return rv;
}

}
