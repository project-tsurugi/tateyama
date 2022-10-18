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
#include <tateyama/framework/transactional_kvs_resource.h>

#include <glog/logging.h>

#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::framework {

bool transactional_kvs_resource::setup(environment& env) {
    auto s = env.configuration()->get_section("datastore");
    BOOST_ASSERT(s != nullptr); //NOLINT
    sharksfin::DatabaseOptions options{};
    if(auto res = s->get<std::string>("log_location"); res) {
        auto location = res.value();
        if(!location.empty()) {
            static constexpr std::string_view KEY_LOCATION{"location"};
            options.attribute(KEY_LOCATION, location);
        }
    }
    if(auto res = s->get<std::size_t>("logging_max_parallelism"); res) {
        auto sz = res.value();
        if(sz > 0) {
            static constexpr std::string_view KEY_LOGGING_MAX_PARALLELISM{"logging_max_parallelism"};
            options.attribute(KEY_LOGGING_MAX_PARALLELISM, std::to_string(sz));
        }
    }
    if(auto res = sharksfin::database_open(options, std::addressof(database_handle_)); res != sharksfin::StatusCode::OK) {
        LOG(ERROR) << "opening database failed";
        return false;
    }
    db_opened_ = true;
    return true;
}

bool transactional_kvs_resource::start(environment&) {
    // no-op
    return true;
}

bool transactional_kvs_resource::shutdown(environment&) {
    if(! db_opened_) {
        return true;
    }
    if(auto res = sharksfin::database_close(database_handle_); res != sharksfin::StatusCode::OK) {
        LOG(ERROR) << "closing database failed";
        // proceed to dispose db even on error
    }
    if(auto res = sharksfin::database_dispose(database_handle_); res != sharksfin::StatusCode::OK) {
        LOG(ERROR) << "disposing database failed";
        return false;
    }
    db_opened_ = false;
    return true;
}

component::id_type transactional_kvs_resource::id() const noexcept {
    return tag;
}

sharksfin::DatabaseHandle transactional_kvs_resource::core_object() const noexcept {
    return database_handle_;
}

transactional_kvs_resource::transactional_kvs_resource(sharksfin::DatabaseHandle handle) noexcept:
    database_handle_(handle)
{}
}

