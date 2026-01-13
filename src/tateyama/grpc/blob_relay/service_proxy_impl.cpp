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

#include <glog/logging.h>

#include <tateyama/utils/boolalpha.h>
#include <tateyama/logging.h>
#include "logging.h"

#include <tateyama/grpc/server_resource.h>

#include "service_handler.h"
#include "service_proxy_impl.h"

namespace tateyama::grpc::blob_relay {

using namespace framework;

std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> service_proxy_impl::blob_relay_service() {
    return service_handler_;
}

service_proxy_impl::service_proxy_impl() = default;

service_proxy_impl::~service_proxy_impl() = default;

bool service_proxy_impl::setup(framework::environment& env) {
    const auto& cfg = env.configuration();

    // grpc section
    bool grpc_enabled{false};
    if (auto* grpc_config = cfg->get_section("grpc_server"); grpc_config) {
        if (auto grpc_enabled_opt = grpc_config->get<bool>("enabled"); grpc_enabled_opt) {
            grpc_enabled = grpc_enabled_opt.value();
        }
    }

    if (grpc_enabled) {
        // check blob_relay.enabled
        if (auto* blob_relay_config = cfg->get_section("blob_relay"); blob_relay_config) {
            if (auto blob_relay_enabled_opt = blob_relay_config->get<bool>("enabled"); blob_relay_enabled_opt) {
                blob_relay_enabled_ = blob_relay_enabled_opt.value();

                // output configuration to be used
                LOG(INFO) << blob_relay_config_prefix
                          << "enabled: " << utils::boolalpha(blob_relay_enabled_) << ", "
                          << "blob relay service is enabled or not.";

                if (blob_relay_enabled_) {
                    try {
                        using data_relay_grpc::blob_relay::blob_session;

                        datastore_resource_ = env.resource_repository().find<tateyama::datastore::resource::bridge>();
                        if (!datastore_resource_) {
                            return false;
                        }
                        api_ = std::make_unique<data_relay_grpc::blob_relay::blob_relay_service::api>(
                            [this](blob_session::blob_id_type bid, blob_session::transaction_id_type tid) {
                                return datastore_resource_->datastore().generate_reference_tag(bid, tid); },
                            [this](blob_session::blob_id_type bid) {
                                return std::filesystem::path{datastore_resource_->datastore().get_blob_file(bid).path().native()};
                            }
                        );
                        // create the relay service
                        service_handler_ = service_handler::create_service(blob_relay_config, *api_);
                        if (!service_handler_) {
                            LOG(ERROR) << "cannot start the blob relay service";
                            return false;
                        }

                        if (auto sr = env.resource_repository().find<tateyama::grpc::grpc_server_resource>(); sr) {
                            for(auto&& e: service_handler_->services()) {
                                sr->add_service(e);
                            }
                        }

                    } catch (std::exception &ex) {
                        LOG(ERROR) << ex.what();
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

}
