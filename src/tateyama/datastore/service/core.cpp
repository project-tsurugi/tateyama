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
#include <tateyama/datastore/service/core.h>

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>
#ifdef ENABLE_ALTIMETER
#include "altimeter_logger.h"
#endif

namespace tateyama::datastore::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

bool tateyama::datastore::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    namespace ns = tateyama::proto::datastore::request;

    constexpr static auto this_request_does_not_use_session_id = static_cast<std::size_t>(-2);

    auto data = req->payload();
    ns::Request rq{};
    if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        LOG(ERROR) << "request parse error";
        return false;
    }

    DVLOG(log_debug) << "request is no. " << rq.command_case();
    switch(rq.command_case()) {
        case ns::Request::kBackupBegin: {
            backup_id_++;
            backup_ = std::make_unique<limestone_backup>(resource_->begin_backup());
#ifdef ENABLE_ALTIMETER
            service::backup(req, "all", backup_restore_success);
#endif

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
        case ns::Request::kBackupDetailBegin: {
            auto& rb = rq.backup_detail_begin();
            auto type = (rb.type() == ns::BackupType::STANDARD) ?
                limestone::api::backup_type::standard :
                limestone::api::backup_type::transaction;
            backup_detail_ = resource_->begin_backup(type);
#ifdef ENABLE_ALTIMETER
            service::backup(req,
                    type == limestone::api::backup_type::standard ? "standard" : "transaction",
                    backup_restore_success);
#endif

            tateyama::proto::datastore::response::BackupBegin rp{};
            auto success = rp.mutable_success();
            success->set_id(backup_id_);
            auto detail_source = success->mutable_detail_source();
            detail_source->set_log_begin(backup_detail_->log_start());
            detail_source->set_log_end(backup_detail_->log_finish());
            if (auto image_finish = backup_detail_->image_finish(); image_finish) {
                detail_source->set_image_finish_value(image_finish.value());
            } else {
                detail_source->set_image_finish_is_not_set(0);
            }
            auto entries = detail_source->mutable_detail_files();
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
            success->clear_detail_source();
            rp.clear_success();
            break;
        }
        case ns::Request::kBackupEnd: {
            tateyama::proto::datastore::response::BackupEnd rp{};
            resource_->end_backup();
            if (backup_ || backup_detail_) {
                if (backup_) {
                    backup_->backup().notify_end_backup();
                }
                if (backup_detail_) {
                    backup_detail_->notify_end_backup();
                }
                rp.mutable_success();
                res->session_id(req->session_id());
                auto body = rp.SerializeAsString();
                res->body(body);
                rp.clear_success();
                backup_ = nullptr;
                backup_detail_ = nullptr;
            } else {
                rp.mutable_expired();
            }
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
        case ns::Request::kRestoreBegin: {
            auto& rb = rq.restore_begin();
            tateyama::proto::datastore::response::RestoreBegin rp{};
            limestone::status rc{};
#ifdef ENABLE_ALTIMETER
            std::string command{};
#endif
            switch (rb.source_case()) {
            case ns::RestoreBegin::kBackupDirectory:
                rc = resource_->restore_backup(rb.backup_directory(), rb.keep_backup());
#ifdef ENABLE_ALTIMETER
                command = "directory " + rb.backup_directory();
#endif
                break;
            case ns::RestoreBegin::kTagName:
                LOG(ERROR) << "restore tag is not implemented";
                break;
            case ns::RestoreBegin::kEntries:
            {
                std::vector<limestone::api::file_set_entry> entries{};
                for (auto&& f: rb.entries().file_set_entry()) {
                    entries.emplace_back(boost::filesystem::path(f.source_path()),
                                         boost::filesystem::path(f.destination_path()),
                                         f.detached());
                }
                rc = resource_->restore_backup(rb.entries().directory(), entries);
#ifdef ENABLE_ALTIMETER
                command = "entries " + rb.entries().directory();
#endif
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
#ifdef ENABLE_ALTIMETER
            service::restore(req, command,
                    rc == limestone::status::ok ? backup_restore_success : backup_restore_fail);
#endif
            res->session_id(this_request_does_not_use_session_id);
            auto body = rp.SerializeAsString();
            res->body(body);
            rp.clear_success();
            break;
        }

    case ns::Request::kTagList:
    case ns::Request::kTagAdd:
    case ns::Request::kTagGet:
    case ns::Request::kTagRemove:
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
