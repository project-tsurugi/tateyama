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

bool tateyama::datastore::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    // mock implementation TODO
    namespace ns = tateyama::proto::datastore::request;

    constexpr static auto this_request_does_not_use_session_id = static_cast<std::size_t>(-2);

    auto data = req->payload();
    ns::Request rq{};
    if(! rq.ParseFromArray(data.data(), data.size())) {
        VLOG(log_error) << "request parse error";
        return false;
    }

    VLOG(log_debug) << "request is no. " << rq.command_case();
    switch(rq.command_case()) {
        case ns::Request::kBackupBegin: {
            resource_->begin_backup();

            auto files = resource_->list_backup_files();
            tateyama::proto::datastore::response::BackupBegin rp{};
            auto success = rp.mutable_success();
            for(auto&& f : files) {
                success->add_files(f.string());
            }
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kBackupEnd: {
            resource_->end_backup();

            tateyama::proto::datastore::response::BackupEnd rp{};
            rp.mutable_success();
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kBackupContine: {
            tateyama::proto::datastore::response::BackupContinue rp{};
            rp.mutable_success();
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kBackupEstimate: {
            tateyama::proto::datastore::response::BackupEstimate rp{};
            auto success = rp.mutable_success();
            success->set_number_of_files(123);
            success->set_number_of_bytes(456);
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kRestoreBackup: {
            tateyama::proto::datastore::response::RestoreBackup rp{};
            rp.mutable_success();
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }

#if 0
    case ns::Request::kRestoreTag: {
            tateyama::proto::datastore::response::RestoreTag rp{};
            rp.mutable_success();
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kTagList: {
            auto tags = resource_->list_tags();
            tateyama::proto::datastore::response::TagList rp{};
            auto success = rp.mutable_success();
            auto t = success->mutable_tags();
            for(auto&& tag : tags) {
                t->Add()->set_name(tag);
            }
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            success->clear_tags();
            rp.clear_success();
            break;
        }
        case ns::Request::kTagAdd: {
            auto& ta = rq.tag_add();
            auto info = resource_->add_tag(ta.name(), ta.comment());
            tateyama::proto::datastore::response::TagAdd rp{};
            auto success = rp.mutable_success();
            auto t = success->mutable_tag();
            t->set_name(info.name_);
            t->set_comment(info.comment_);
            t->set_author(info.author_);
            t->set_timestamp(info.timestamp_);
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            success->clear_tag();
            rp.clear_success();
            break;
        }
        case ns::Request::kTagGet: {
            auto& tg = rq.tag_get();
            resource::tag_info info{};
            auto found = resource_->get_tag(tg.name(), info);
            tateyama::proto::datastore::response::TagGet rp{};
            if(found) {
                auto success = rp.mutable_success();
                auto t = success->mutable_tag();
                t->set_name(info.name_);
                t->set_comment(info.comment_);
                t->set_author(info.author_);
                t->set_timestamp(info.timestamp_);
                res->session_id(req->session_id());
                auto body = rp.SerializeAsString();
                res->body(body);
                success->clear_tag();
                rp.clear_success();
            } else {
                auto not_found = rp.mutable_not_found();
                not_found->set_name(tg.name());
                res->session_id(req->session_id());
                auto body = rp.SerializeAsString();
                res->body(body);
                rp.clear_not_found();
            }
            break;
        }
        case ns::Request::kTagRemove: {
            auto& tr = rq.tag_remove();
            auto found = resource_->remove_tag(tr.name());
            tateyama::proto::datastore::response::TagRemove rp{};
            if(found) {
                auto success = rp.mutable_success();
                success->set_name(tr.name());
                res->session_id(req->session_id());
                auto body = rp.SerializeAsString();
                res->body(body);
                rp.clear_success();
            } else {
                auto not_found = rp.mutable_not_found();
                not_found->set_name(tr.name());
                res->session_id(req->session_id());
                auto body = rp.SerializeAsString();
                res->body(body);
                rp.clear_not_found();
            }
            break;
        }
#else
    case ns::Request::kRestoreTag:
    case ns::Request::kTagList:
    case ns::Request::kTagAdd:
    case ns::Request::kTagGet:
    case ns::Request::kTagRemove:
        break;
#endif
        case ns::Request::COMMAND_NOT_SET: break;
    }
    return true;
}

core::core(std::shared_ptr<tateyama::api::configuration::whole> cfg) :
    cfg_(std::move(cfg))
{}

bool core::start(tateyama::datastore::resource::bridge* resource) {
    resource_ = resource;
    //TODO implement
    return true;
}

bool core::shutdown(bool force) {
    //TODO implement
    (void) force;
    return true;
}

}
