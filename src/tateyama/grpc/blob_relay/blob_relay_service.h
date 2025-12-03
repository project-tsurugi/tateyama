/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include <grpcpp/grpcpp.h>
#include <glog/logging.h>

#include <limestone/api/datastore.h>

#include <tateyama/utils/boolalpha.h>
#include <tateyama/logging.h>
#include <tateyama/datastore/resource/bridge.h>
#include <tateyama/grpc/logging.h>
#include <tateyama/framework/environment.h>
#include <tateyama/grpc/server/service_handler.h>

#include <data-relay-grpc/blob_relay/service.h>

namespace tateyama::grpc::blob_relay {

using data_relay_grpc::blob_relay::blob_session;

class blob_relay_service_handler : public tateyama::grpc::server::grpc_service_handler {
public:
    blob_relay_service_handler() = default;

    bool start(::tateyama::framework::environment& env) {
        auto datastore = env.resource_repository().find<tateyama::datastore::resource::bridge>();
        if (!datastore) {
            return false;
        }
        datastore_handler_ = std::make_unique<datastore_handler>(datastore->datastore());
        service_ = std::make_shared<data_relay_grpc::blob_relay::blob_relay_service>(
            data_relay_grpc::blob_relay::blob_relay_service::api(
                [this](blob_session::blob_id_type bid, blob_session::transaction_id_type tid) {
                    return datastore_handler_->generate_reference_tag(bid, tid); },
                [this](blob_session::blob_id_type bid) {
                    return datastore_handler_->get_blob_file(bid);
                }),
            blob_relay_service_configuration(env));
        return service_ != nullptr;
    }
    void register_to_builder(::grpc::ServerBuilder& builder) override {
        service_->add_blob_relay_service(builder);
    }
    std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service() {
        return service_;
    }

private:
    class datastore_handler {
      public:
        explicit datastore_handler(::limestone::api::datastore& datastore) : datastore_(datastore), blob_pool_(datastore.acquire_blob_pool()) {
        }
        blob_session::blob_tag_type generate_reference_tag(blob_session::blob_id_type bid, blob_session::transaction_id_type tid) {
            return blob_pool_->generate_reference_tag(bid, tid);
        }
        blob_session::blob_path_type get_blob_file(blob_session::blob_id_type bid) {
            auto bf = datastore_.get_blob_file(bid);
            return { bf.path().c_str() };
        }

      private:
        ::limestone::api::datastore& datastore_;
        std::unique_ptr<limestone::api::blob_pool> blob_pool_;
    };

    std::unique_ptr<datastore_handler> datastore_handler_{};
    std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> service_{};

    static data_relay_grpc::blob_relay::service_configuration blob_relay_service_configuration(::tateyama::framework::environment& env) {
        auto* blob_relay_config = env.configuration()->get_section("blob_relay");

        std::filesystem::path session_store{};
        std::size_t session_quota_size{};
        bool local_enabled{};
        bool local_upload_copy_file{};
        std::size_t stream_chunk_size{};
        bool dev_accept_mock_tag{};

        if (auto session_store_opt = blob_relay_config->get<std::filesystem::path>("session_store"); session_store_opt) {
            session_store = session_store_opt.value();
        }
        if (auto session_quota_size_opt = blob_relay_config->get<std::size_t>("session_quota_size"); session_quota_size_opt) {
            session_quota_size = session_quota_size_opt.value();
        }
        if (auto local_enabled_opt = blob_relay_config->get<bool>("local_enabled"); local_enabled_opt) {
            local_enabled = local_enabled_opt.value();
        }
        if (auto local_upload_copy_file_opt = blob_relay_config->get<bool>("local_upload_copy_file"); local_upload_copy_file_opt) {
            local_upload_copy_file = local_upload_copy_file_opt.value();
        }
        if (auto stream_chunk_size_opt = blob_relay_config->get<std::size_t>("stream_chunk_size"); stream_chunk_size_opt) {
            stream_chunk_size = stream_chunk_size_opt.value();
        }
        if (auto dev_accept_mock_tag_opt = blob_relay_config->get<bool>("dev_accept_mock_tag"); dev_accept_mock_tag_opt) {
            dev_accept_mock_tag = dev_accept_mock_tag_opt.value();
        }

        // output configuration to be used
        LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
                  << "session_store: " << session_store.c_str() << ", "
                  << "location of the session_store.";
        LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
                  << "session_quota_size: " << session_quota_size << ", "
                  << "session quota size.";
        LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
                  << "local_enabled: " << utils::boolalpha(local_enabled) << ", "
                  << "enable data transfer using the file system or not.";
        LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
                  << "local_upload_copy_file: " << utils::boolalpha(local_upload_copy_file) << ", "
                  << "whether to copy the original file when uploading using the file system.";
        LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
                  << "stream_chunk_size: " << stream_chunk_size << ", "
                  << "chunk size (in bytes) when transferring data in chunks during download in gRPC streaming.";
        LOG(INFO) << tateyama::grpc::blob_relay_config_prefix
                  << "dev_accept_mock_tag: " << utils::boolalpha(dev_accept_mock_tag) << ", "
                  << "whether to accept mock tag.";
        return { session_store, session_quota_size, local_enabled, local_upload_copy_file, stream_chunk_size, dev_accept_mock_tag };
    }
};

} // namespace
