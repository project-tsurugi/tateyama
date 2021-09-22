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

#include "ipc_response.h"

namespace tateyama::common::wire {


// class ipc_response
tateyama::status ipc_response::body(std::string_view body) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    if(body.length() > response_box::response::max_response_message_length) {
        return tateyama::status::unknown;
    }
    memcpy(response_box_.get_buffer(body.length()), body.data(), body.length());
    return tateyama::status::ok;
}

tateyama::status ipc_response::complete() {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    ipc_request_.dispose();
    if (response_code_ != tateyama::api::endpoint::response_code::started || acquire_channel_or_complete_) {
        response_box_.flush();
        return tateyama::status::ok;
    }
    acquire_channel_or_complete_ = true;
    return tateyama::status::ok;
}

void ipc_response::code(tateyama::api::endpoint::response_code code) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    response_code_ = code;
}

void ipc_response::message(std::string_view msg) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    message_ = msg;
}

tateyama::status ipc_response::acquire_channel(std::string_view name, tateyama::api::endpoint::data_channel*& ch) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    data_channel_ = std::make_unique<ipc_data_channel>(server_wire_.create_resultset_wires(name));
    if (ch = data_channel_.get(); ch != nullptr) {
        if (acquire_channel_or_complete_) {
            response_box_.flush();
        } else {
            acquire_channel_or_complete_ = true;
        }
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_response::release_channel(tateyama::api::endpoint::data_channel& ch) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

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
    VLOG(1) << __func__ << std::endl;  //NOLINT

    server_wire_.close_session();
    return tateyama::status::ok;
}

// class ipc_data_channel
tateyama::status ipc_data_channel::acquire(tateyama::api::endpoint::writer*& wrt) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    if (auto ipc_wrt = std::make_unique<ipc_writer>(data_channel_->acquire()); ipc_wrt != nullptr) {
        wrt = ipc_wrt.get();
        data_writers_.emplace(std::move(ipc_wrt));
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_data_channel::release(tateyama::api::endpoint::writer& wrt) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    dynamic_cast<ipc_writer*>(std::addressof(wrt))->resultset_wire_->eor();
    if (auto itr = data_writers_.find(dynamic_cast<ipc_writer*>(&wrt)); itr != data_writers_.end()) {
        data_writers_.erase(itr);
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

// class writer
tateyama::status ipc_writer::write(char const* data, std::size_t length) {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    resultset_wire_->write(data, length);
    return tateyama::status::ok;
}

tateyama::status ipc_writer::commit() {
    VLOG(1) << __func__ << std::endl;  //NOLINT

    return tateyama::status::ok;
}

}  // tateyama::common::wire
