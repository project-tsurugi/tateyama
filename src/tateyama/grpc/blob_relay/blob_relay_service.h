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

#include <tateyama/logging.h>
#include <tateyama/utils/boolalpha.h>
#include <tateyama/grpc/logging.h>
#include <tateyama/framework/environment.h>
#include <tateyama/grpc/server/service_handler.h>

#include <data-relay-grpc/blob_relay/services.h>

namespace tateyama::grpc::blob_relay {

class blob_relay_service : public tateyama::grpc::server::grpc_service_handler {
public:
    blob_relay_service(::limestone::api::datastore& datastore, ::tateyama::framework::environment& env) :
        datastore_(datastore),
        configuration_(blob_relay_service_configuration(env)),
        services_(
            data_relay_grpc::blob_relay::services::api(
                [this](std::uint64_t, std::uint64_t){ return 0; },
                [this](std::uint64_t bid){ return std::filesystem::path(datastore_.get_blob_file(bid).path().c_str()); }
            ),
            configuration_
        ) {
    }
    void register_to_builder(::grpc::ServerBuilder& builder) override {
        services_.add_blob_relay_services(builder);
    }

    data_relay_grpc::blob_relay::blob_session& create_session(std::optional<blob_relay_service_resource::transaction_id_type> transaction_id) {
        return services_.create_session(transaction_id);
    }
    
private:
    ::limestone::api::datastore& datastore_;
    data_relay_grpc::blob_relay::service_configuration configuration_;
    data_relay_grpc::blob_relay::services services_;

    static data_relay_grpc::blob_relay::service_configuration blob_relay_service_configuration(::tateyama::framework::environment& env) {
        auto* blob_relay_config = env.configuration()->get_section("blob_relay");

        std::filesystem::path session_store{};
        std::size_t session_quota_size{};
        bool local_enabled{};
        bool local_upload_copy_file{};
        std::size_t stream_chunk_size{};

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
        return { session_store, session_quota_size, local_enabled, local_upload_copy_file, stream_chunk_size };
    }
};

} // namespace
