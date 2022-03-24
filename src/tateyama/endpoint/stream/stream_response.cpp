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

namespace tateyama::common::stream {

class stream_request;

// class stream_response
stream_response::stream_response(stream_request& request, unsigned char index, std::size_t session_id)
    : session_socket_(request.get_session_socket()), index_(index), session_id_(session_id) {
}

tateyama::status stream_response::body(std::string_view body) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    session_socket_.send(stream_socket::RESPONSE_SESSION_PAYLOAD, index_, body);
    return tateyama::status::ok;
}

tateyama::status stream_response::body_head(std::string_view body_head) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    session_socket_.send(stream_socket::RESPONSE_SESSION_PAYLOAD, index_, body_head);
    return tateyama::status::ok;
}

void stream_response::code(tateyama::api::endpoint::response_code code) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    response_code_ = code;
}

tateyama::status stream_response::acquire_channel(std::string_view name, tateyama::api::endpoint::data_channel*& ch) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    data_channel_ = std::make_unique<stream_data_channel>(session_socket_);
    std::string data_channel_name = std::string(name.data(), name.length());
    if (ch = data_channel_.get(); ch != nullptr) {
        session_socket_.register_resultset(data_channel_name, data_channel_.get());
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status stream_response::release_channel(tateyama::api::endpoint::data_channel& ch) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (data_channel_.get() == dynamic_cast<stream_data_channel*>(&ch)) {
        session_socket_.send(stream_socket::RESPONSE_RESULT_SET_PAYLOAD, index_, 0, "");
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
tateyama::status stream_data_channel::acquire(tateyama::api::endpoint::writer*& wrt) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    {
        std::unique_lock lock{mutex_};
        if (!index_is_valid_) {
            condition_.wait(lock, [&](){ return index_is_valid_; });
        }
    }

    if (auto stream_wrt = std::make_unique<stream_writer>(session_socket_, index_, writer_id_++); stream_wrt != nullptr) {
        wrt = stream_wrt.get();
        data_writers_.emplace(std::move(stream_wrt));
        index_++;
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status stream_data_channel::release(tateyama::api::endpoint::writer& wrt) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    if (auto itr = data_writers_.find(dynamic_cast<stream_writer*>(&wrt)); itr != data_writers_.end()) {
        data_writers_.erase(itr);
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

void stream_data_channel::set_slot(unsigned char slot) {
    {
        std::unique_lock lock{mutex_};
        index_ = slot;
        index_is_valid_ = true;
    }
    condition_.notify_one();
}

// class writer
tateyama::status stream_writer::write(char const* data, std::size_t length) {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    resultset_socket_.send(stream_socket::RESPONSE_RESULT_SET_PAYLOAD, index_, writer_id_, std::string_view(data, length));
    return tateyama::status::ok;
}

tateyama::status stream_writer::commit() {
    VLOG(log_trace) << __func__ << std::endl;  //NOLINT

    return tateyama::status::ok;
}

}  // tateyama::common::stream
