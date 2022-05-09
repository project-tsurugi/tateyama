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
#include <tateyama/datastore/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

namespace tateyama::datastore::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

void tateyama::datastore::service::core::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    // mock implementation TODO
    namespace ns = tateyama::proto::datastore::request;

    auto data = req->payload();
    ns::Request rq{};
    if(! rq.ParseFromArray(data.data(), data.size())) {
        VLOG(log_error) << "request parse error";
        return;
    }

    switch(rq.command_case()) {
        case ns::Request::kBackupBegin: {
            auto files = resource_->list_backup_files();
            tateyama::proto::datastore::response::BackupBegin rp{};
            auto success = rp.mutable_success();
            for(auto&& f : files) {
                success->add_files(f);
            }
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kBackupEnd: break;
        case ns::Request::kBackupContine: break;
        case ns::Request::kBackupEstimate: break;
        case ns::Request::kRestoreBackup: break;
        case ns::Request::kRestoreTag: break;
        case ns::Request::kTagList: break;
        case ns::Request::kTagAdd: break;
        case ns::Request::kTagGet: break;
        case ns::Request::kTagRemove: break;
        case ns::Request::COMMAND_NOT_SET: break;
    }
}

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{}

void core::start(tateyama::datastore::resource::core* resource) {
    resource_ = resource;
    //TODO implement
}

void core::shutdown(bool force) {
    //TODO implement
    (void) force;
}

}

