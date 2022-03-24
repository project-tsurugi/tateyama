/*
 * Copyright 2019-2022 tsurugi project.
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
#include "stream.h"
#include "stream_response.h"

namespace tateyama::common::stream {

bool stream_socket::search_and_setup_resultset(std::string_view name, unsigned char slot) {  // for REQUEST_RESULTSET_HELLO
    auto itr = resultset_relations_.find(std::string(name));
    if (itr == resultset_relations_.end()) {
        return false;
    }
    auto* data_channel = itr->second;
    resultset_relations_.erase(itr);
    data_channel->set_slot(slot);
    return true;
}

};
