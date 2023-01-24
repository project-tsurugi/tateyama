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
#pragma once

#include <tateyama/framework/component.h>

namespace tateyama::framework {

// resource
constexpr inline component::id_type resource_id_task_scheduler = 0;
constexpr inline component::id_type resource_id_status = 1;
constexpr inline component::id_type resource_id_transactional_kvs = 2;
constexpr inline component::id_type resource_id_sql = 3;
constexpr inline component::id_type resource_id_datastore = 4;
//constexpr inline component::id_type resource_id_session = 5;
//constexpr inline component::id_type resource_id_mutex = 6;

// service
constexpr inline component::id_type service_id_routing = 0;
constexpr inline component::id_type service_id_endpoint_broker = 1;
constexpr inline component::id_type service_id_datastore = 2;
constexpr inline component::id_type service_id_sql = 3;
constexpr inline component::id_type service_id_fdw = 4;

}
