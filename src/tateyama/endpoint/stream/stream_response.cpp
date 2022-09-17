/*
 * Copyright 2019-2022 tsurugi project.
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

#include "stream_response.h"
#include "../common/endpoint_proto_utils.h"

namespace tateyama::common::stream {

class stream_request;

// class stream_response
stream_response::stream_response(stream_request& request, unsigned char index)
    : session_socket_(request.get_session_socket()), index_(index) {
}

tateyama::status stream_response::body(std::string_view body) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT
    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body, arg); ! res) {
        VLOG(log_error) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    session_socket_.send(index_, s);
    return tateyama::status::ok;
}

tateyama::status stream_response::body_head(std::string_view body_head) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT
    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg); ! res) {
        VLOG(log_error) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    session_socket_.send(index_, s);
    return tateyama::status::ok;
}

void stream_response::code(tateyama::api::endpoint::response_code code) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    response_code_ = code;
}

tateyama::status stream_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    auto slot = session_socket_.look_for_slot();
    data_channel_ = std::make_unique<stream_data_channel>(session_socket_, slot);
    if (ch = data_channel_; ch != nullptr) {
        session_socket_.send_result_set_hello(slot, name);
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status stream_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (auto dc = dynamic_cast<stream_data_channel*>(&ch); data_channel_.get() == dc) {
        auto slot = dc->get_slot();
        session_socket_.send_result_set_bye(slot);
        data_channel_ = nullptr;
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status stream_response::close_session() {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    session_socket_.close_session();
    return tateyama::status::ok;
}

// class stream_data_channel
tateyama::status stream_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (auto stream_wrt = std::make_shared<stream_writer>(session_socket_, get_slot(), writer_id_++); stream_wrt != nullptr) {
        wrt = stream_wrt;
        data_writers_.emplace(std::move(stream_wrt));
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status stream_data_channel::release(tateyama::api::server::writer& wrt) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (auto itr = data_writers_.find(dynamic_cast<stream_writer*>(&wrt)); itr != data_writers_.end()) {
        data_writers_.erase(itr);
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

// class writer
tateyama::status stream_writer::write(char const* data, std::size_t length) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    resultset_socket_.send(slot_, writer_id_, std::string_view(data, length));
    return tateyama::status::ok;
}

tateyama::status stream_writer::commit() {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    return tateyama::status::ok;
}

}  // tateyama::common::stream
