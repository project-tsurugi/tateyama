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
#include <tateyama/proto/diagnostics.pb.h>

#include "stream_response.h"
#include "../common/endpoint_proto_utils.h"

namespace tateyama::common::stream {

class stream_request;

// class stream_response
stream_response::stream_response(stream_request& request, unsigned char index)
    : session_socket_(request.get_session_socket()), index_(index) {
}

tateyama::status stream_response::body(std::string_view body) {
    VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_) << " length = " << body.length();  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body, arg); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    session_socket_.send(index_, s, true);
    return tateyama::status::ok;
}

tateyama::status stream_response::body_head(std::string_view body_head) {
    VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_);  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    session_socket_.send(index_, s, false);
    return tateyama::status::ok;
}

void stream_response::server_diagnostics(std::string_view diagnostic_record) {
    VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_);  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(! endpoint::common::append_response_header(ss, diagnostic_record, arg, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS)) {
        LOG_LP(ERROR) << "error formatting response message";
        return;
    }
    auto s = ss.str();
    session_socket_.send(index_, s, true);
}

void stream_response::code(tateyama::api::server::response_code code) {
    VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_);  //NOLINT

    response_code_ = code;
}

tateyama::status stream_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) {
    try {
        auto slot = session_socket_.look_for_slot();
        data_channel_ = std::make_unique<stream_data_channel>(session_socket_, slot);
        VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

        if (ch = data_channel_; ch != nullptr) {
            session_socket_.send_result_set_hello(slot, name);
            return tateyama::status::ok;
        }
    } catch (std::exception &ex) {
        LOG_LP(ERROR) << ex.what();

        ::tateyama::proto::diagnostics::Record record{};
        record.set_code(::tateyama::proto::diagnostics::Code::RESOURCE_LIMIT);
        record.set_message("error in acquire_channel");
        std::string s{};
        if(record.SerializeToString(&s)) {
            server_diagnostics(s);
        } else {
            LOG_LP(ERROR) << "error formatting diagnostics message";
            server_diagnostics("");
        }
        record.release_message();
    }
    return tateyama::status::unknown;
}

tateyama::status stream_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    if (auto dc = dynamic_cast<stream_data_channel*>(&ch); data_channel_.get() == dc) {
        auto slot = dc->get_slot();
        session_socket_.send_result_set_bye(slot);
        data_channel_ = nullptr;
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}


// deprecated
tateyama::status stream_response::close_session() {
    VLOG_LP(log_trace) << static_cast<const void*>(&session_socket_);  //NOLINT

    return tateyama::status::ok;
}

// class stream_data_channel
tateyama::status stream_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    if (auto stream_wrt = std::make_shared<stream_writer>(session_socket_, get_slot(), writer_id_.fetch_add(1)); stream_wrt != nullptr) {
        wrt = stream_wrt;
        VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(wrt.get());  //NOLINT

        {
            std::unique_lock lock{mutex_};
            data_writers_.emplace(std::move(stream_wrt));
        }
        return tateyama::status::ok;
    }
    return tateyama::status::unknown;
}

tateyama::status stream_data_channel::release(tateyama::api::server::writer& wrt) {
    VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(&wrt);  //NOLINT
    {
        std::unique_lock lock{mutex_};
        if (auto itr = data_writers_.find(dynamic_cast<stream_writer*>(&wrt)); itr != data_writers_.end()) {
            data_writers_.erase(itr);
            return tateyama::status::ok;
        }
    }
    return tateyama::status::unknown;
}

// class writer
tateyama::status stream_writer::write(char const* data, std::size_t length) {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT

    resultset_socket_.send(slot_, writer_id_, std::string_view(data, length));
    return tateyama::status::ok;
}

tateyama::status stream_writer::commit() {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT

    resultset_socket_.send(slot_, writer_id_, std::string_view());
    return tateyama::status::ok;
}

}  // tateyama::common::stream
