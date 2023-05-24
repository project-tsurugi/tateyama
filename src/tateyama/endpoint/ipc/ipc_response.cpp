/*
 * Copyright 2019-2023 tsurugi project.
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

#include "ipc_response.h"
#include "../common/endpoint_proto_utils.h"

namespace tateyama::common::wire {


// class server_wire
tateyama::status ipc_response::body(std::string_view body) {
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_) << " length = " << body.length();  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body, arg); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_.get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODY));
    mark_end();
    return tateyama::status::ok;
}

tateyama::status ipc_response::body_head(std::string_view body_head) {
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_);  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_.get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODYHEAD));
    return tateyama::status::ok;
}

void ipc_response::code(tateyama::api::server::response_code code) {
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_);  //NOLINT

    response_code_ = code;
}

tateyama::status ipc_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) {
    data_channel_ = std::make_shared<ipc_data_channel>(server_wire_.create_resultset_wires(name));
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    if (ch = data_channel_; ch != nullptr) {
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

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
    VLOG_LP(log_trace) << static_cast<const void*>(&server_wire_);  //NOLINT

    server_wire_.close_session();
    return tateyama::status::ok;
}

// class ipc_data_channel
tateyama::status ipc_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    if (auto ipc_wrt = std::make_shared<ipc_writer>(data_channel_->acquire()); ipc_wrt != nullptr) {
        wrt = ipc_wrt;
        VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(wrt.get());  //NOLINT
        {
            std::unique_lock lock{mutex_};
            data_writers_.emplace(std::move(ipc_wrt));
        }
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
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

    resultset_wire_->write(data, length);
    return tateyama::status::ok;
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
