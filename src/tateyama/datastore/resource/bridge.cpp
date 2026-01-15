/*
 * Copyright 2018-2026 Project Tsurugi.
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
#include <tateyama/configuration/configuration_provider.h>

namespace tateyama::datastore::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

static bool extract_config(environment& env, limestone::api::configuration& options) {
    try {
        std::string location{};
        int recover_param = 0;
        if(auto ds = env.configuration()->get_section("datastore")) {
            if (auto res = ds->get<std::filesystem::path>("log_location"); res) {
                if(res->empty()) {
                    // if value is empty, it's not relative path and is invalid
                    LOG(ERROR) << "datastore log_location configuration parameter is empty";
                    return false;
                }
                location = res->string();
            }
            if (auto res = ds->get<int>("recover_max_parallelism"); res) {
                auto sz = res.value();
                if(sz > 0) {
                    recover_param = sz;
                }
            }
        }
        options = limestone::api::configuration();
        options.set_data_location(location);
        options.set_recover_max_parallelism(recover_param);
        if (auto configuration = env.resource_repository().find<configuration::configuration_provider>(); configuration) {
            options.set_instance_id(configuration->database_info().instance_id());
        } else {
            LOG(WARNING) << "cannot find configuration::configuration_provider, and thus instance_id has not been provided to datastore";
            return false;
        }
    } catch(std::exception const& e) {
        // error log should have been made
        return false;
    }
    return true;
}

bool bridge::setup(environment& env) {
    return extract_config(env, config_);
}

bool bridge::start(environment&) {
    try {
        datastore_ = std::make_shared<limestone::api::datastore>(config_);
    } catch (std::runtime_error &ex) {
        LOG(ERROR) << "opening datastore failed";
        return false;
    }
    return true;
}

bool bridge::shutdown(environment&) {
    datastore_.reset();
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

limestone::api::datastore& bridge::datastore() const noexcept {
    return *datastore_;
}

std::string_view bridge::label() const noexcept {
    return component_label;
}

}
