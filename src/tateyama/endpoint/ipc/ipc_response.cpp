/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <exception>
#include <string>

#include <glog/logging.h>

#include <tateyama/logging.h>
#include <tateyama/proto/diagnostics.pb.h>

#include "ipc_response.h"
#include "tateyama/endpoint/common/endpoint_proto_utils.h"

namespace tateyama::common::wire {


// class server_wire
tateyama::status ipc_response::body(std::string_view body) {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " length = " << body.length();  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body, arg); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_->get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODY));
    return tateyama::status::ok;
}

tateyama::status ipc_response::body_head(std::string_view body_head) {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get());  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_->get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODYHEAD));
    return tateyama::status::ok;
}

void ipc_response::error(proto::diagnostics::Record const& record) {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get());  //NOLINT

    std::string s{};
    if(record.SerializeToString(&s)) {
        server_diagnostics(s);
    } else {
        LOG_LP(ERROR) << "error formatting diagnostics message";
        server_diagnostics("");
    }
}

void ipc_response::server_diagnostics(std::string_view diagnostic_record) {
    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(! endpoint::common::append_response_header(ss, diagnostic_record, arg, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS)) {
        LOG_LP(ERROR) << "error formatting response message";
        return;
    }
    auto s = ss.str();
    server_wire_->get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODY));
}

void ipc_response::code(tateyama::api::server::response_code code) {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get());  //NOLINT

    response_code_ = code;
}

tateyama::status ipc_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) {
    try {
        data_channel_ = std::make_shared<ipc_data_channel>(server_wire_->create_resultset_wires(name));
    } catch (std::runtime_error &ex) {
        LOG_LP(INFO) << "Running out of shared memory for result set transfers. Probably due to too many result sets being opened";

        ch = nullptr;
        return tateyama::status::unknown;
    }
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    if (ch = data_channel_; ch != nullptr) {
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    data_channel_->set_eor();
    if (data_channel_.get() == dynamic_cast<ipc_data_channel*>(&ch)) {
        if (!data_channel_->is_closed()) {
            garbage_collector_->put(data_channel_->get_resultset_wires());
        }
        data_channel_ = nullptr;
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_response::close_session() {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get());  //NOLINT

    server_wire_->close_session();
    return tateyama::status::ok;
}

// class ipc_data_channel
tateyama::status ipc_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    try {
        if (auto ipc_wrt = std::make_shared<ipc_writer>(data_channel_->acquire()); ipc_wrt != nullptr) {
            wrt = ipc_wrt;
            VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(wrt.get());  //NOLINT
            {
                std::unique_lock lock{mutex_};
                data_writers_.emplace(std::move(ipc_wrt));
            }
            return tateyama::status::ok;
        }
        throw std::runtime_error("error in create ipc_writer");
    } catch (std::runtime_error &ex) {
        LOG_LP(INFO) << "Running out of shared memory for result set transfers. Probably due to too many result sets being opened";

        wrt = nullptr;
        return tateyama::status::unknown;
    }
}

tateyama::status ipc_data_channel::release(tateyama::api::server::writer& wrt) {
    VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(&wrt);  //NOLINT
    {
        std::unique_lock lock{mutex_};
        if (auto itr = data_writers_.find(dynamic_cast<ipc_writer*>(&wrt)); itr != data_writers_.end()) {
            (*itr)->release();
            data_writers_.erase(itr);
            return tateyama::status::ok;
        }
    }
    return tateyama::status::unknown;
}

// class writer
tateyama::status ipc_writer::write(char const* data, std::size_t length) {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT

    try {
        resultset_wire_->write(data, length);
        return tateyama::status::ok;
    } catch (std::runtime_error &ex) {
        LOG_LP(ERROR) << ex.what();
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_writer::commit() {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT

    resultset_wire_->flush();
    return tateyama::status::ok;
}

void ipc_writer::release() {
    resultset_wire_->release(std::move(resultset_wire_));
}

}  // tateyama::common::wire
