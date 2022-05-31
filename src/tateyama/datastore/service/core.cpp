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

core::core(
    std::shared_ptr<tateyama::api::configuration::whole> cfg,
    tateyama::datastore::resource::core* resource
) :
    cfg_(std::move(cfg)),
    resource_(resource)
{}

bool core::operator()(proto::datastore::request::BackupBegin const&, proto::datastore::response::BackupBegin& rp) {
    auto files = resource_->list_backup_files();
    auto success = rp.mutable_success();
    for(auto&& f : files) {
        success->add_files(f);
    }
    return true;
}

bool core::operator()(proto::datastore::request::BackupEnd const&, proto::datastore::response::BackupEnd& rp) {
    rp.mutable_success();
    return true;
}

bool core::operator()(proto::datastore::request::BackupContinue const&, proto::datastore::response::BackupContinue& rp) {
    rp.mutable_success();
    return true;
}

bool core::operator()(proto::datastore::request::BackupEstimate const&, proto::datastore::response::BackupEstimate& rp) {
    auto success = rp.mutable_success();
    success->set_number_of_files(123);
    success->set_number_of_bytes(456);
    return true;
}

bool core::operator()(proto::datastore::request::RestoreBackup const&, proto::datastore::response::RestoreBackup& rp) {
    rp.mutable_success();
    return true;
}

bool core::operator()(proto::datastore::request::RestoreTag const&, proto::datastore::response::RestoreTag& rp) {
    rp.mutable_success();
    return true;
}

bool core::operator()(proto::datastore::request::TagList const&, proto::datastore::response::TagList& rp) {
    auto tags = resource_->list_tags();
    auto success = rp.mutable_success();
    auto t = success->mutable_tags();
    for(auto&& tag : tags) {
        t->Add()->set_name(tag);
    }
    return true;
}

bool core::operator()(proto::datastore::request::TagAdd const& ta, proto::datastore::response::TagAdd& res) {
    auto info = resource_->add_tag(ta.name(), ta.comment());
    auto success = res.mutable_success();
    auto t = success->mutable_tag();
    t->set_name(info.name_);
    t->set_comment(info.comment_);
    t->set_author(info.author_);
    t->set_timestamp(info.timestamp_);
    return true;
}

bool core::operator()(proto::datastore::request::TagGet const& tg, proto::datastore::response::TagGet& rp) {
    resource::tag_info info{};
    auto found = resource_->get_tag(tg.name(), info);
    if(found) {
        auto success = rp.mutable_success();
        auto t = success->mutable_tag();
        t->set_name(info.name_);
        t->set_comment(info.comment_);
        t->set_author(info.author_);
        t->set_timestamp(info.timestamp_);
    } else {
        auto not_found = rp.mutable_not_found();
        not_found->set_name(tg.name());
    }
    return true;
}

bool core::operator()(proto::datastore::request::TagRemove const& tr, proto::datastore::response::TagRemove& rp) {
    auto found = resource_->remove_tag(tr.name());
    if(found) {
        auto success = rp.mutable_success();
        success->set_name(tr.name());
    } else {
        auto not_found = rp.mutable_not_found();
        not_found->set_name(tr.name());
    }
    return true;
}

}

