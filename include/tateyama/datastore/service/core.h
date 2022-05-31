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

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>
#include <tateyama/datastore/resource/core.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

namespace tateyama::datastore::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief datastore service main object
 */
class core {
public:
    core() = default;

    core(
        std::shared_ptr<tateyama::api::configuration::whole> cfg,
        tateyama::datastore::resource::core* resource
    );

    bool operator()(
        tateyama::proto::datastore::request::BackupBegin const&,
        tateyama::proto::datastore::response::BackupBegin& rp
    );

    bool operator()(
        tateyama::proto::datastore::request::BackupEnd const&,
        tateyama::proto::datastore::response::BackupEnd& rp
    );

    bool operator()(
        tateyama::proto::datastore::request::BackupContinue const&,
        tateyama::proto::datastore::response::BackupContinue& rp
    );

    bool operator()(
        tateyama::proto::datastore::request::BackupEstimate const&,
        tateyama::proto::datastore::response::BackupEstimate& rp
    );

    bool operator()(
        tateyama::proto::datastore::request::RestoreBackup const&,
        tateyama::proto::datastore::response::RestoreBackup& rp
    );

    bool operator()(
        tateyama::proto::datastore::request::RestoreTag const& ,
        tateyama::proto::datastore::response::RestoreTag& rp
    );

    bool operator()(
        tateyama::proto::datastore::request::TagList const&,
        tateyama::proto::datastore::response::TagList & rp
    );

    bool operator()(
        tateyama::proto::datastore::request::TagAdd const& ta,
        tateyama::proto::datastore::response::TagAdd & res
    );

    bool operator()(
        tateyama::proto::datastore::request::TagGet const& tg,
        tateyama::proto::datastore::response::TagGet & rp
    );

    bool operator()(
        tateyama::proto::datastore::request::TagRemove const& tr,
        tateyama::proto::datastore::response::TagRemove & rp
    );

private:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
    tateyama::datastore::resource::core* resource_{};
};

}

