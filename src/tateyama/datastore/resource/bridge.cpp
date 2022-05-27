/*
 * Copyright 2018-2022 tsurugi project.
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
#include "tateyama/datastore/resource/bridge.h"

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::datastore::resource {

using namespace framework;

limestone::api::datastore* get_datastore() {
    ::sharksfin::Slice id{};
    if(auto rc = ::sharksfin::implementation_id(std::addressof(id)); rc != ::sharksfin::StatusCode::OK) {
        std::abort();
    }
    if(id.to_string_view() == "shirakami") {
        void* dsp = ::shirakami::get_datastore();
        return reinterpret_cast<limestone::api::datastore*>(dsp);
    }
    return nullptr;
}

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup([[maybe_unused]] environment& env) {
    return true;
}

bool bridge::start(environment&) {
    datastore_ = get_datastore();
    return datastore_ != nullptr;
}

bool bridge::shutdown(environment&) {
    deactivated_ = true;
    return true;
}

bridge::~bridge() {
}

#if 0
std::vector<std::string> bridge::list_backup_files() {
    // mock implementation TODO
    return std::vector<std::string>{
        "/var/datastore/file1",
        "/var/datastore/file2",
        "/var/datastore/file3",
    };
}

std::vector<std::string> bridge::list_tags() {
    std::vector<std::string> ret{};
    ret.reserve(tags_.size());
    for(auto&& [name, comment] : tags_) {
        (void)comment;
        ret.emplace_back(name);
    }
    return ret;
}

// tag_info bridge::add_tag(std::string_view name, std::string_view comment) {
void bridge::add_tag(std::string_view name, std::string_view comment) {
    auto& tag_repository = datastore_->epoch_tag_repository();

    tag_repository.register_tag(std::string(name), std::string(comment));
    // TODO fill author and timestamp correctly
//    tag_info t{std::string(name), std::string{comment}, "author", 100000};
//    tags_.emplace(n, t);
//    return t;
}

bool bridge::get_tag(std::string_view name, tag_info& out) {
    std::string n{name};
    if(auto it = tags_.find(n); it != tags_.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool bridge::remove_tag(std::string_view name) {
    std::string n{name};
    if(auto it = tags_.find(n); it != tags_.end()) {
        tags_.erase(it);
        return true;
    }
    return false;
}
#endif

}
