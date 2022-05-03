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

#include <tateyama/framework/ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::framework {

void transactional_kvs_resource::setup(environment&) {
    //TODO
}

void transactional_kvs_resource::start(environment&) {
    //TODO
}

void transactional_kvs_resource::shutdown(environment&) {
    //TODO
}

component::id_type transactional_kvs_resource::id() const noexcept {
    return tag;
}

sharksfin::DatabaseHandle transactional_kvs_resource::handle() const noexcept {
    return database_handle_;
}

transactional_kvs_resource::transactional_kvs_resource(sharksfin::DatabaseHandle handle) noexcept:
    database_handle_(handle)
{}
}
