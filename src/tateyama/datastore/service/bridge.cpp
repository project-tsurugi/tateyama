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
#include <tateyama/datastore/service/bridge.h>

#include <functional>
#include <memory>
#include <type_traits>

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/datastore/resource/bridge.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

namespace tateyama::datastore::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment&) {
    return true;
}

bool bridge::start(environment& env) {
    auto resource = env.resource_repository().find<tateyama::datastore::resource::bridge>();
    if (! resource) {
        LOG(ERROR) << "datastore resource not found";
        return false;
    }
    core_ = std::make_unique<core>(env.configuration(), resource->core_object());
    return true;
}

bool bridge::shutdown(environment&) {
    core_.reset();
    return true;
}


bool bridge::operator()(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    constexpr static auto this_request_does_not_use_session_id = static_cast<std::size_t>(-2);
    namespace ns = tateyama::proto::datastore::request;
    auto data = req->payload();
    ns::Request rq{};
    if(! rq.ParseFromArray(data.data(), data.size())) {
        VLOG(log_error) << "request parse error";
        return false;
    }

    VLOG(log_debug) << "request is no. " << rq.command_case();

    switch(rq.command_case()) {
        case ns::Request::kBackupEstimate:
        case ns::Request::kRestoreBackup:
        case ns::Request::kRestoreTag:
            res->session_id(this_request_does_not_use_session_id);
            break;
        default:
            res->session_id(req->session_id());
            break;
    }
    namespace resp = tateyama::proto::datastore::response;
    switch(rq.command_case()) {
        case ns::Request::kBackupBegin: return process<resp::BackupBegin>(rq.backup_begin(), *res);
        case ns::Request::kBackupEnd: return process<resp::BackupEnd>(rq.backup_end(), *res);
        case ns::Request::kBackupContine: return process<resp::BackupContinue>(rq.backup_contine(), *res);
        case ns::Request::kBackupEstimate: return process<resp::BackupEstimate>(rq.backup_estimate(), *res);
        case ns::Request::kRestoreBackup: return process<resp::RestoreBackup>(rq.restore_backup(), *res);
        case ns::Request::kRestoreTag: return process<resp::RestoreTag>(rq.restore_tag(), *res);
        case ns::Request::kTagList: return process<resp::TagList>(rq.tag_list(), *res);
        case ns::Request::kTagAdd: return process<resp::TagAdd>(rq.tag_add(), *res);
        case ns::Request::kTagGet: return process<resp::TagGet>(rq.tag_get(), *res);
        case ns::Request::kTagRemove: return process<resp::TagRemove>(rq.tag_remove(), *res);
        case ns::Request::COMMAND_NOT_SET: break;
    }
    return false;
}

}

