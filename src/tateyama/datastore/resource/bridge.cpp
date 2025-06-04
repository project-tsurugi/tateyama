/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <any>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/transactional_kvs_resource.h>

namespace tateyama::datastore::resource {

using namespace framework;

limestone::api::datastore* get_datastore(transactional_kvs_resource& kvs) {
    std::any ptr{};
    auto res = ::sharksfin::implementation_get_datastore(kvs.core_object(), std::addressof(ptr));
    if(res == ::sharksfin::StatusCode::OK) {
        return reinterpret_cast<limestone::api::datastore*>(std::any_cast<void*>(ptr));  //NOLINT
    }
    return nullptr;
}

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup([[maybe_unused]] environment& env) {
    return true;
}

bool bridge::start(environment& env) {
    auto kvs = env.resource_repository().find<framework::transactional_kvs_resource>();
    if (! kvs) {
        std::abort();
    }
    datastore_ = get_datastore(*kvs); // this can be nullptr if kvs doesn't support datastore (e.g. memory)
    return true;
}

bool bridge::shutdown(environment&) {
    deactivated_ = true;
    return true;
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

limestone::api::backup& bridge::begin_backup() {
    return datastore_->begin_backup();
}

std::unique_ptr<limestone::api::backup_detail> bridge::begin_backup(limestone::api::backup_type type) {
    return datastore_->begin_backup(type);
}

void bridge::end_backup() {
}

limestone::status bridge::restore_backup(std::string_view from, bool keep_backup) {
    return datastore_->restore(from, keep_backup);
}

limestone::status bridge::restore_backup(std::string_view from, std::vector<limestone::api::file_set_entry>& entries) {
    return datastore_->restore(from, entries);
}

std::string_view bridge::label() const noexcept {
    return component_label;
}

}
