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

#include "tateyama/session/resource/core_impl.h"

namespace tateyama::session::resource {

session_container& sessions_core_impl::sessions() noexcept {
    return container_;
}

session_container const& sessions_core_impl::sessions() const noexcept {
    return container_;
}

session_variable_declaration_set& sessions_core_impl::variable_declarations() noexcept {
    return variable_declarations_;
}

session_variable_declaration_set const& sessions_core_impl::variable_declarations() const noexcept {
    return variable_declarations_;
}

}
