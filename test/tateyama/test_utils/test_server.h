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
#pragma once

#include <tateyama/framework/server.h>
#include <tateyama/framework/repository.h>

#include <tateyama/status/resource/bridge.h>
#include <tateyama/framework/routing_service.h>
#include <tateyama/session/service/bridge.h>
#include <tateyama/session/resource/bridge.h>
#include <tateyama/metrics/service/bridge.h>
#include <tateyama/metrics/resource/bridge.h>
#ifdef ENABLE_ALTIMETER
#include <tateyama/altimeter/service/bridge.h>
#endif
#include <tateyama/request/service/bridge.h>
#include <tateyama/endpoint/ipc/bootstrap/ipc_endpoint.h>

namespace tateyama::test_utils {

    static void add_core_components_for_test(tateyama::framework::server& svr) {
        svr.add_resource(std::make_shared<tateyama::metrics::resource::bridge>());
        svr.add_resource(std::make_shared<tateyama::status_info::resource::bridge>());
        svr.add_resource(std::make_shared<tateyama::session::resource::bridge>());

        svr.add_service(std::make_shared<tateyama::framework::routing_service>());
        svr.add_service(std::make_shared<tateyama::metrics::service::bridge>());
        svr.add_service(std::make_shared<tateyama::session::service::bridge>());
#ifdef ENABLE_ALTIMETER
        svr.add_service(std::make_shared<altimeter::service::bridge>());
#endif
        svr.add_service(std::make_shared<tateyama::request::service::bridge>());

        svr.add_endpoint(std::make_shared<tateyama::framework::ipc_endpoint>());
    }

}
