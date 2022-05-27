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
#include <tateyama/datastore/resource/core.h>
#include <shirakami/interface.h>
#include <sharksfin/api.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::datastore::resource {

#if 0 // TODO uncomment when shirakami implemented get_datastore()
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
#endif

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{
}

bool core::start() {
    //TODO implement
// TODO uncomment when shirakami implemented get_datastore()
//    datastore_ = get_datastore();
    datastore_ = new limestone::api::datastore();
    return true;
}

bool core::shutdown(bool force) {
    //TODO implement
    (void) force;
    return true;
}

std::vector<std::string> core::list_backup_files() {
    // mock implementation TODO
    return std::vector<std::string>{
        "/var/datastore/file1",
        "/var/datastore/file2",
        "/var/datastore/file3",
    };
}

std::vector<std::string> core::list_tags() {
    std::vector<std::string> ret{};
    ret.reserve(tags_.size());
    for(auto&& [name, comment] : tags_) {
        (void)comment;
        ret.emplace_back(name);
    }
    return ret;
}

// tag_info core::add_tag(std::string_view name, std::string_view comment) {
void core::add_tag(std::string_view name, std::string_view comment) {
    auto& tag_repository = datastore_->epoch_tag_repository();

    tag_repository.register_tag(std::string(name), std::string(comment));
    // TODO fill author and timestamp correctly
//    tag_info t{std::string(name), std::string{comment}, "author", 100000};
//    tags_.emplace(n, t);
//    return t;
}

bool core::get_tag(std::string_view name, tag_info& out) {
    std::string n{name};
    if(auto it = tags_.find(n); it != tags_.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool core::remove_tag(std::string_view name) {
    std::string n{name};
    if(auto it = tags_.find(n); it != tags_.end()) {
        tags_.erase(it);
        return true;
    }
    return false;
}

}
