/*
 * Copyright 2018-2020 tsurugi project.
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
#include "service.h"

#include <string_view>
#include <memory>

#include <msgpack.hpp>
#include <takatori/util/downcast.h>
#include <takatori/util/fail.h>

#include <jogasaki/status.h>
#include <jogasaki/api/database.h>
#include <jogasaki/configuration.h>
#include <jogasaki/api/statement_handle.h>

#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>

#include "schema.pb.h"
#include "request.pb.h"
#include "response.pb.h"
#include "common.pb.h"

namespace tateyama::api::endpoint::impl {

std::unique_ptr<service> create_service(jogasaki::api::database& db) {
    return std::make_unique<impl::service>(db);
}

}