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

#include <glog/logging.h>

#include <takatori/util/assertion.h>

#include <tateyama/logging.h>
#include <tateyama/api/endpoint/request.h>
#include <tateyama/api/endpoint/response.h>
#include <tateyama/server/impl/request.h>
#include <tateyama/server/impl/response.h>

#include <tateyama/common.h>

namespace tateyama::api::endpoint {

std::shared_ptr<service> create_service(tateyama::api::environment& env) {
    // assuming only one application exists
    BOOST_ASSERT(env.applications().size() > 0);  //NOLINT
    return std::make_shared<impl::service>(env.applications()[0]);
}

impl::service::service(std::shared_ptr<api::server::service> app) :
    application_(std::move(app))
{}

tateyama::status impl::service::operator()(
    std::shared_ptr<tateyama::api::endpoint::request const> req,
    std::shared_ptr<tateyama::api::endpoint::response> res
) {
    auto svrreq = tateyama::api::server::impl::create_request(std::move(req));
    if(! svrreq) {
        VLOG(log_error) << "request transfer error";
        return status::unknown;  //TODO assign error code
    }
    return application_->operator()(
        std::move(svrreq),
        std::make_shared<tateyama::api::server::impl::response>(std::move(res))
    );
}

}
