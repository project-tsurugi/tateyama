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
#pragma once

#include <string_view>
#include <atomic>
#include <memory>

#include <takatori/util/downcast.h>
#include <takatori/util/fail.h>

#include <jogasaki/api/database.h>
#include <jogasaki/api/transaction_handle.h>
#include <jogasaki/api/statement_handle.h>

#include <tateyama/status.h>
#include <tateyama/api/endpoint/service.h>
#include <tateyama/api/endpoint/response.h>
#include <tateyama/api/endpoint/writer.h>
#include <tateyama/api/endpoint/data_channel.h>

#include "request.pb.h"
#include "response.pb.h"
#include "common.pb.h"
#include "common.pb.h"

namespace tateyama::api::endpoint::impl {

using takatori::util::unsafe_downcast;
using takatori::util::fail;

class service : public api::endpoint::service {
public:
    service() = default;

    explicit service(jogasaki::api::database& db) : db_(std::addressof(db)) {}

    tateyama::status operator()(
        std::shared_ptr<tateyama::api::endpoint::request const> req,
        std::shared_ptr<tateyama::api::endpoint::response> res
    ) override {
        return (*db_)(req, res);
    }

private:

    jogasaki::api::database* db_{};
};

}

