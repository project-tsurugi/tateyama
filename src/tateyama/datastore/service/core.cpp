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
    namespace ns = tateyama::proto::datastore::request;

    constexpr static auto this_request_does_not_use_session_id = static_cast<std::size_t>(-2);

    auto data = req->payload();
    ns::Request rq{};
    if(!rq.ParseFromArray(data.data(), data.size())) {
        LOG(ERROR) << "request parse error";
        return false;
    }

    DVLOG(log_debug) << "request is no. " << rq.command_case();
    switch(rq.command_case()) {
        case ns::Request::kBackupBegin: {
            backup_id_++;
            backup_ = std::make_unique<limestone_backup>(resource_->begin_backup());

            tateyama::proto::datastore::response::BackupBegin rp{};
            auto success = rp.mutable_success();
            success->set_id(backup_id_);
            auto simple_source = success->mutable_simple_source();
            for(auto&& f : backup_->backup().files()) {
                simple_source->add_files(f.string());
            }
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
        case ns::Request::kDifferentialBackupBegin: {
            auto& rb = rq.differential_backup_begin();
            auto type = (rb.type() == ns::BackupType::STANDARD) ?
                limestone::api::backup_type::standard :
                limestone::api::backup_type::transaction;
            backup_detail_ = resource_->begin_backup(type);

            tateyama::proto::datastore::response::BackupBegin rp{};
            auto success = rp.mutable_success();
            success->set_id(backup_id_);
            auto differential_source = success->mutable_differential_source();
            differential_source->set_log_begin(backup_detail_->log_start());
            differential_source->set_log_end(backup_detail_->log_finish());
            if (auto image_finish = backup_detail_->image_finish(); image_finish) {
                differential_source->set_image_finish(image_finish.value());
            }
            auto entries = differential_source->mutable_differential_files();
            for (auto&& e : backup_detail_->entries()) {
                auto* entry = entries->Add();
                entry->set_source(e.source_path().string());
                entry->set_destination(e.destination_path().string());
                entry->set_mutable_(e.is_mutable());
                entry->set_detached(e.is_detached());
            }
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            success->clear_differential_source();
            rp.clear_success();
            break;
            break;
        }
        case ns::Request::kBackupEnd: {
            tateyama::proto::datastore::response::BackupEnd rp{};
            if (backup_) {
                rp.mutable_success();
                res->session_id(req->session_id());
                auto body = rp.SerializeAsString();
                res->body(body);
                rp.clear_success();
                backup_ = nullptr;
            } else {
                rp.mutable_expired();
            }
            break;
        }
#if 0
        case ns::Request::kBackupContine: {
            tateyama::proto::datastore::response::BackupContinue rp{};
            rp.mutable_success();
            res->session_id(req->session_id());
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }
#endif
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
        case ns::Request::kRestoreBegin: {
            auto& rb = rq.restore_begin();
            tateyama::proto::datastore::response::RestoreBegin rp{};
            limestone::status rc{};
            switch (rb.source_case()) {
            case ns::RestoreBegin::kBackupDirectory:
                rc = resource_->restore_backup(rb.backup_directory(), rb.keep_backup());
                break;
            case ns::RestoreBegin::kTagName:
                LOG(ERROR) << "restore tag is not implemented";
                break;
            case ns::RestoreBegin::kEntries:
            {
                std::vector<limestone::api::file_set_entry> entries{};
                for (auto&& f: rb.entries().file_set_entry()) {
                    entries.emplace_back(limestone::api::file_set_entry(
                                             boost::filesystem::path(f.source_path()),
                                             boost::filesystem::path(f.destination_path()),
                                             f.detached()
                                         )
                    );
                }
                rc = resource_->restore_backup(rb.entries().directory(), entries);
                break;
            }
            default:
                LOG(ERROR) << "source is not specified";
                break;
            }
            switch (rc) {
            case limestone::status::ok:
                rp.mutable_success();
                break;
            case limestone::status::err_not_found:
                rp.mutable_not_found();
                break;
            case limestone::status::err_permission_error:
                rp.mutable_permission_error();
                break;
            case limestone::status::err_broken_data:
                rp.mutable_broken_data();
                break;
            default:
                rp.mutable_unknown_error();
                break;
            }
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }

#if 0
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
    case ns::Request::kTagList:
    case ns::Request::kTagAdd:
    case ns::Request::kTagGet:
    case ns::Request::kTagRemove:
#endif
    case ns::Request::kRestoreStatus:
    case ns::Request::kRestoreCancel:
    case ns::Request::kRestoreDispose:
        break;
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
