/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include <vector>
#include <list>

#include <tateyama/grpc/blob_session.h>
#include "blob_session_impl.h"

namespace tateyama::grpc {

blob_session::blob_session(std::unique_ptr<blob_session_impl, void(*)(blob_session_impl*)> impl)
    : impl_(std::move(impl)) {
}

blob_session::session_id_type blob_session::session_id() const noexcept {
    return impl_->session_id();
}

bool blob_session::is_valid() const noexcept {
    return impl_->is_valid();
}

void blob_session::dispose() {
    impl_->dispose();
}

blob_session::blob_id_type blob_session::add(blob_path_type path) {
    return impl_->add(path);
}

std::optional<blob_session::blob_path_type> blob_session::find(blob_id_type blob_id) const {
    return impl_->find(blob_id);
}

std::vector<blob_session::blob_id_type> blob_session::entries() const {
    return impl_->entries();
}

using vector_iterator = std::vector<blob_session::blob_id_type>::iterator;
template <>
void blob_session::remove<vector_iterator>(vector_iterator blob_ids_begin, vector_iterator blob_ids_end) {
    impl_->remove(blob_ids_begin, blob_ids_end);
}

using list_iterator = std::list<blob_session::blob_id_type>::iterator;
template <>
void blob_session::remove<list_iterator>(list_iterator blob_ids_begin, list_iterator blob_ids_end) {
    impl_->remove(blob_ids_begin, blob_ids_end);
}

} // namespace
