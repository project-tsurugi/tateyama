/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <filesystem>
#include <exception>
#include <glog/logging.h>

#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::framework {

static bool extract_config(environment& env, sharksfin::DatabaseOptions& options) {
    if(env.mode() == boot_mode::maintenance_server || env.mode() == boot_mode::maintenance_standalone) {
        static constexpr std::string_view KEY_STARTUP_MODE{"startup_mode"};
        options.attribute(KEY_STARTUP_MODE, "maintenance");
    }
    try {
        if(auto ds = env.configuration()->get_section("datastore")) {
            if (auto res = ds->get<std::filesystem::path>("log_location"); res) {
                if(res->empty()) {
                    // if value is empty, it's not relative path and is invalid
                    LOG(ERROR) << "datastore log_location configuration parameter is empty";
                    return false;
                }
                // sharksfin db location name is different for historical reason
                static constexpr std::string_view KEY_LOCATION{"location"};
                options.attribute(KEY_LOCATION, res->string());
            }
            if(auto res = ds->get<int>("recover_max_parallelism"); res) {
                auto sz = res.value();
                if(sz > 0) {
                    static constexpr std::string_view KEY_RECOVER_MAX_PARALLELISM{"recover_max_parallelism"};
                    options.attribute(KEY_RECOVER_MAX_PARALLELISM, std::to_string(sz));
                }
            }
        }
        if(auto cc = env.configuration()->get_section("cc")) {
            if(auto res = cc->get<std::size_t>("epoch_duration"); res) {
                auto sz = res.value();
                if(sz > 0) {
                    static constexpr std::string_view KEY_EPOCH_DURATION{"epoch_duration"};
                    options.attribute(KEY_EPOCH_DURATION, std::to_string(sz));
                }
            }
            if(auto res = cc->get<std::size_t>("waiting_resolver_threads"); res) {
                auto sz = res.value();
                if(sz > 0) {
                    static constexpr std::string_view KEY_WAITING_RESOLVER_THREADS{"waiting_resolver_threads"};
                    options.attribute(KEY_WAITING_RESOLVER_THREADS, std::to_string(sz));
                }
            }
            if(auto res = cc->get<std::size_t>("index_restore_threads"); res) {
                auto sz = res.value();
                if(sz > 0) {
                    static constexpr std::string_view KEY_INDEX_RESTORE_THREADS{"index_restore_threads"};
                    options.attribute(KEY_INDEX_RESTORE_THREADS, std::to_string(sz));
                }
            }
        }
    } catch(std::exception const& e) {
        // error log should have been made
        return false;
    }
    return true;
}

bool transactional_kvs_resource::setup(environment& env) {
    sharksfin::DatabaseOptions options{};
    if(! extract_config(env, options)) {
        return false;
    }
    try {
        if(auto res = sharksfin::database_open(options, std::addressof(database_handle_)); res != sharksfin::StatusCode::OK) {
            LOG(ERROR) << "opening database failed";
            return false;
        }
        db_opened_ = true;
        return true;
    } catch (std::runtime_error &ex) {
        LOG(ERROR) << "opening database failed";
        return false;
    }
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
        // normally closing db won't return error. Even if it happens, proceed to clean up even on error.
    }
    if(auto res = sharksfin::database_dispose(database_handle_); res != sharksfin::StatusCode::OK) {
        LOG(ERROR) << "disposing database failed";
        // proceed to clean up even on error
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

std::string_view transactional_kvs_resource::label() const noexcept {
    return component_label;
}

transactional_kvs_resource::~transactional_kvs_resource() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}
}

