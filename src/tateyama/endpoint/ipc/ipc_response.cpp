/*
 * Copyright 2019-2019 tsurugi project.
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


// class ipc_response
tateyama::status ipc_response::body(std::string_view body) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT
    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body, arg); ! res) {
        VLOG(log_error) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_.get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODY));
    response_box_.flush();
    return tateyama::status::ok;
}

tateyama::status ipc_response::body_head(std::string_view body_head) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT
    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg); ! res) {
        VLOG(log_error) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_.get_response_wire().write(s.data(), response_header(index_, s.length(), RESPONSE_BODYHEAD));
    response_box_.flush();
    return tateyama::status::ok;
}

void ipc_response::code(tateyama::api::server::response_code code) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    response_code_ = code;
}

tateyama::status ipc_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    data_channel_ = std::make_shared<ipc_data_channel>(server_wire_.create_resultset_wires(name));
    if (ch = data_channel_; ch != nullptr) {
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

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
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    server_wire_.close_session();
    return tateyama::status::ok;
}

// class ipc_data_channel
tateyama::status ipc_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (auto ipc_wrt = std::make_shared<ipc_writer>(data_channel_->acquire()); ipc_wrt != nullptr) {
        wrt = ipc_wrt;
        data_writers_.emplace(std::move(ipc_wrt));
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_data_channel::release(tateyama::api::server::writer& wrt) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (auto itr = data_writers_.find(dynamic_cast<ipc_writer*>(&wrt)); itr != data_writers_.end()) {
        data_writers_.erase(itr);
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

// class writer
tateyama::status ipc_writer::write(char const* data, std::size_t length) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    resultset_wire_->write(data, length);
    return tateyama::status::ok;
}

tateyama::status ipc_writer::commit() {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    resultset_wire_->flush();
    return tateyama::status::ok;
}

}  // tateyama::common::wire
