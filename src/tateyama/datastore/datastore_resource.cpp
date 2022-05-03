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
#include <tateyama/datastore/datastore_resource.h>

#include <tateyama/framework/ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::datastore {

using namespace framework;

component::id_type datastore_resource::id() const noexcept {
    return tag;
}

void datastore_resource::setup(environment&) {
    //TODO
}

void datastore_resource::start(environment&) {
    //TODO
}

void datastore_resource::shutdown(environment&) {
    //TODO
}

}
