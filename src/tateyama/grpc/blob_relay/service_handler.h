/*
 * Copyright 2025-2026 Project Tsurugi.
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

#include <data_relay_grpc/blob_relay/service.h>

#include <glog/logging.h>

#include <tateyama/utils/boolalpha.h>
#include <tateyama/logging.h>
#include "logging.h"
#include <tateyama/api/configuration.h>

namespace tateyama::grpc::blob_relay {

class service_handler {
public:
    static std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> create_service(
        api::configuration::section* blob_relay_config,
        data_relay_grpc::blob_relay::blob_relay_service::api& api
    ) {
        return std::make_shared<data_relay_grpc::blob_relay::blob_relay_service>(api, blob_relay_service_configuration(blob_relay_config));
    }

private:
    static data_relay_grpc::blob_relay::service_configuration blob_relay_service_configuration(api::configuration::section* blob_relay_config) {
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

        LOG(INFO) << blob_relay_config_prefix
                  << "session_store: " << session_store.c_str() << ", "
                  << "location of the session_store.";
        LOG(INFO) << blob_relay_config_prefix
                  << "session_quota_size: " << session_quota_size << ", "
                  << "session quota size.";
        LOG(INFO) << blob_relay_config_prefix
                  << "local_enabled: " << utils::boolalpha(local_enabled) << ", "
                  << "enable data transfer using the file system or not.";
        LOG(INFO) << blob_relay_config_prefix
                  << "local_upload_copy_file: " << utils::boolalpha(local_upload_copy_file) << ", "
                  << "whether to copy the original file when uploading using the file system.";
        LOG(INFO) << blob_relay_config_prefix
                  << "stream_chunk_size: " << stream_chunk_size << ", "
                  << "chunk size (in bytes) when transferring data in chunks during download in gRPC streaming.";
        LOG(INFO) << blob_relay_config_prefix
                  << "dev_accept_mock_tag: " << utils::boolalpha(dev_accept_mock_tag) << ", "
                  << "whether to accept mock tag.";
        return { session_store, session_quota_size, local_enabled, local_upload_copy_file, stream_chunk_size, dev_accept_mock_tag };
    }
};

} // namespace
