/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <string>

#include <tateyama/api/configuration.h>
#include <tateyama/proto/blob_relay_privilege/request.pb.h>
#include <tateyama/proto/blob_relay_privilege/response.pb.h>

#include <tateyama/blob_relay_privilege/service/core.h>

namespace tateyama::blob_relay_privilege::service {

/**
 * @brief Handles a blob_relay_privilege service request.
 *
 * This function processes the incoming request and populates the response accordingly.
 *
 * @param req The incoming request object.
 * @param res The response object to populate.
 * @return true if the request was handled successfully; false otherwise.
 */
bool tateyama::blob_relay_privilege::service::core::operator()(const std::shared_ptr<request>& req, const std::shared_ptr<response>& res) {
    using namespace std::literals::string_literals;

    if (!activated_) {
        send_diagnostics(res, tateyama::proto::diagnostics::Code::SERVICE_UNAVAILABLE, "ipc_endpoint.allow_blob_privileged in the configuration is false"s);
        return false;
    }

    try {
        tateyama::proto::blob_relay_privilege::request::Request rq{};

        auto data = req->payload();
        if(!rq.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            std::string msg = "request parse error"s;
            LOG(INFO) << msg;
            send_diagnostics(res, tateyama::proto::diagnostics::Code::SYSTEM_ERROR, msg);
            return false;
        }

        switch(rq.command_case()) {
        case tateyama::proto::blob_relay_privilege::request::Request::kGetBlob:
        {
            tateyama::proto::blob_relay_privilege::response::GetBlob rs{};
            auto& cmd = rq.get_blob();
            auto tid = cmd.transaction_handle();
            auto& blob_reference = cmd.blob_reference();
            auto storage_id = blob_reference.storage_id();
            if (storage_id != LIMESTONE_BLOB_STORE) {
                std::string msg = "storage_id must be LIMESTONE_BLOB_STORE"s;
                LOG(INFO) << msg;
                send_error<tateyama::proto::blob_relay_privilege::response::GetBlob>(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, msg);
                return false;
            }
            auto object_id = blob_reference.object_id();
            auto tag = blob_reference.tag();
            auto ctag = datastore_resource_->datastore().generate_reference_tag(object_id, tid);
            if (tag != ctag) {
                LOG(INFO) << "tag mismatch, given = "s + std::to_string(tag) + " and calculated = "s + std::to_string(ctag);
                send_error<tateyama::proto::blob_relay_privilege::response::GetBlob>(res, tateyama::proto::diagnostics::Code::PERMISSION_ERROR, "tag mismatch");
                return false;
            }
            auto blob_file = datastore_resource_->datastore().get_blob_file(object_id).path().native();

            if (auto* mutable_success = rs.mutable_success(); mutable_success) {
                mutable_success->set_server_file_path(blob_file);
                res->body(rs.SerializeAsString());
                return true;  // success case
            }
            std::string msg = "failed to build response message"s;
            LOG(INFO) << msg;
            send_error<tateyama::proto::blob_relay_privilege::response::GetBlob>(res, tateyama::proto::diagnostics::Code::SYSTEM_ERROR, msg);
            return false;
        }

        case tateyama::proto::blob_relay_privilege::request::Request::COMMAND_NOT_SET:
        default:
        {
            std::string msg = "no valid command"s;
            LOG(INFO) << msg;
            send_diagnostics(res, tateyama::proto::diagnostics::Code::INVALID_REQUEST, msg);
            return false;
        }
        }

    } catch (std::runtime_error &ex) {
        LOG(INFO) << ex.what();
        send_diagnostics(res, tateyama::proto::diagnostics::Code::PERMISSION_ERROR, ex.what());
        return false;
    }
}

core::core(tateyama::framework::environment& env) {
    if (const auto& cfg = env.configuration(); cfg) {
        if (auto* ipc_endpoint_config = cfg->get_section("ipc_endpoint"); ipc_endpoint_config) {
            if (auto blob_opt = ipc_endpoint_config->get<bool>("allow_blob_privileged"); blob_opt) {
                activated_ = blob_opt.value();
            }
        }
    }
}

bool core::start(tateyama::framework::environment& env) {
    datastore_resource_ = env.resource_repository().find<datastore::resource::bridge>();
    if (!datastore_resource_) {
        activated_ = false;
        return false;
    }
    return true;
}

}
