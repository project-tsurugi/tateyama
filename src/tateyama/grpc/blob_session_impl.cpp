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

#include "blob_session_impl.h"

namespace tateyama::grpc {

using vector_iterator = std::vector<blob_session::blob_id_type>::iterator;
template<>
void blob_session_impl::remove<vector_iterator>(vector_iterator blob_ids_begin, vector_iterator blob_ids_end) {
    blob_session_.remove(blob_ids_begin, blob_ids_end);
}

using list_iterator = std::list<blob_session::blob_id_type>::iterator;
template<>
void blob_session_impl::remove<list_iterator>(list_iterator blob_ids_begin, list_iterator blob_ids_end) {
    blob_session_.remove(blob_ids_begin, blob_ids_end);
}

} // namespace
