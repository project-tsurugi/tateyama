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
 * @details this object provide main functionality of datastore service, whose member functions are aligned with
 * datastore proto messages.
 */
class core {
public:
    /**
     * @brief create empty object
     */
    core() = default;

    /**
     * @brief create new object
     * @param cfg configuration
     * @param resource the datastore resource
     */
    core(
        std::shared_ptr<tateyama::api::configuration::whole> cfg,
        tateyama::datastore::resource::core* resource
    );

    /**
     * @brief process BackupBegin message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::BackupBegin const&,
        tateyama::proto::datastore::response::BackupBegin& rp
    );

    /**
     * @brief process BackupEnd message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::BackupEnd const&,
        tateyama::proto::datastore::response::BackupEnd& rp
    );

    /**
     * @brief process BackupContinue message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::BackupContinue const&,
        tateyama::proto::datastore::response::BackupContinue& rp
    );

    /**
     * @brief process BackupEstimate message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::BackupEstimate const&,
        tateyama::proto::datastore::response::BackupEstimate& rp
    );

    /**
     * @brief process RestoreBackup message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::RestoreBackup const&,
        tateyama::proto::datastore::response::RestoreBackup& rp
    );

    /**
     * @brief process RestoreTag message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::RestoreTag const& ,
        tateyama::proto::datastore::response::RestoreTag& rp
    );

    /**
     * @brief process TagList message
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::TagList const&,
        tateyama::proto::datastore::response::TagList & rp
    );

    /**
     * @brief process TagAdd message
     * @param ta request
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::TagAdd const& ta,
        tateyama::proto::datastore::response::TagAdd & res
    );

    /**
     * @brief process TagGet message
     * @param tg request
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::TagGet const& tg,
        tateyama::proto::datastore::response::TagGet & rp
    );

    /**
     * @brief process TagRemove message
     * @param tr request
     * @param rp response to be filled
     * @return true when successful
     * @return false when any error occurs
     */
    bool operator()(
        tateyama::proto::datastore::request::TagRemove const& tr,
        tateyama::proto::datastore::response::TagRemove & rp
    );

private:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
    tateyama::datastore::resource::core* resource_{};
};

}

