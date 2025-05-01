/*
 * Copyright 2018-2025 Project Tsurugi.
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
#include "tateyama/endpoint/common/endpoint_proto_utils.h"

namespace tateyama::endpoint::stream {

// class stream_response
class stream_request;

tateyama::status stream_response::body(std::string_view body) {
    bool expected = false;
    if (complete_gate_.compare_exchange_strong(expected, true)) {
        VLOG_LP(log_trace) << static_cast<const void*>(&stream_) << " length = " << body.length();  //NOLINT
        if (data_channel_) {
            std::dynamic_pointer_cast<stream_data_channel>(data_channel_)->shutdown();  // Guard against improper operation
            data_channel_ = nullptr;
        }

        std::stringstream ss{};
        endpoint::common::header_content arg{};
        arg.session_id_ = session_id_;
        arg.blobs_ = &blobs_;
        if(auto res = endpoint::common::append_response_header(ss, body, arg, ::tateyama::proto::framework::response::Header::SERVICE_RESULT); ! res) {
            LOG_LP(ERROR) << "error formatting response message";
            return status::unknown;
        }
        auto s = ss.str();
        stream_.send(index_, s, true);
        clean_up_();
        set_completed();
        return tateyama::status::ok;
    }
    LOG_LP(ERROR) << "response is already completed";
    return status::unknown;        
}

tateyama::status stream_response::body_head(std::string_view body_head) {
    VLOG_LP(log_trace) << static_cast<const void*>(&stream_);  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg, ::tateyama::proto::framework::response::Header::SERVICE_RESULT); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    stream_.send(index_, s, false);
    set_state(state::to_be_used);
    return tateyama::status::ok;
}

void stream_response::error(proto::diagnostics::Record const& record) {
    bool expected = false;
    if (complete_gate_.compare_exchange_strong(expected, true)) {
        VLOG_LP(log_trace) << static_cast<const void*>(&stream_);  //NOLINT
        if (data_channel_) {
            std::dynamic_pointer_cast<stream_data_channel>(data_channel_)->shutdown();  // Guard against improper operation
            data_channel_ = nullptr;
        }

        std::string s{};
        if(record.SerializeToString(&s)) {
            server_diagnostics(s);
        } else {
            LOG_LP(ERROR) << "error formatting diagnostics message";
            server_diagnostics("");
        }
        clean_up_();
        set_completed();
    } else {
        LOG_LP(ERROR) << "response is already completed";
    }
}

void stream_response::server_diagnostics(std::string_view diagnostic_record) {
    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(! endpoint::common::append_response_header(ss, diagnostic_record, arg, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS)) {
        LOG_LP(ERROR) << "error formatting response message";
        return;
    }
    auto s = ss.str();
    stream_.send(index_, s, true);
}

tateyama::status stream_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch, std::size_t max_writer_count) {
    if (max_writer_count > (UINT8_MAX + 1)) {
        LOG_LP(ERROR) << "too large writer count (" << max_writer_count << ") given";
        set_state(state::acquire_failed);
        return tateyama::status::unknown;
    }
    try {
        data_channel_ = std::make_unique<stream_data_channel>(stream_, index_);
        VLOG_LP(log_trace) << static_cast<const void*>(&stream_) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

        if (ch = data_channel_; ch != nullptr) {
            stream_.send_result_set_hello(index_, name);
            set_state(state::acquired);
            return tateyama::status::ok;
        }
        set_state(state::acquire_failed);
        return tateyama::status::unknown;
    } catch (std::exception &ex) {
        LOG_LP(INFO) << "too many result sets being opened";
        ch = nullptr;
    }
    set_state(state::acquire_failed);
    return tateyama::status::unknown;
}

tateyama::status stream_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG_LP(log_trace) << static_cast<const void*>(&stream_) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    if (auto dc = dynamic_cast<stream_data_channel*>(&ch); data_channel_.get() == dc) {
        dc->shutdown();
        data_channel_ = nullptr;
        set_state(state::released);
        return tateyama::status::ok;
    }
    set_state(state::release_failed);
    return tateyama::status::unknown;
}


// class stream_data_channel
tateyama::status stream_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    try {
        if (auto stream_wrt = std::make_shared<stream_writer>(stream_, get_slot(), writer_id_.fetch_add(1), *this); stream_wrt != nullptr) {
            wrt = stream_wrt;
            VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(wrt.get());  //NOLINT

            {
                std::unique_lock lock{mutex_};
                data_writers_.emplace(std::move(stream_wrt));
            }
            return tateyama::status::ok;
        }
        throw std::runtime_error("error in create stream_writer");
    } catch (std::exception &ex) {
        LOG_LP(INFO) << "too many result sets being opened";
        wrt = nullptr;
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

void stream_data_channel::shutdown() {
    stream_.send_result_set_bye(get_slot());
}

// class writer
tateyama::status stream_writer::write(char const* data, std::size_t length) {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT

    if (data != nullptr && length > 0) {
        if (buffer_.capacity() < buffer_.size() + length) {
            buffer_.reserve(buffer_.size() + length + buffer_size);
        }
        buffer_.insert(buffer_.end(), data, data + length); //NOLINT
    }
    return tateyama::status::ok;
}

tateyama::status stream_writer::commit() {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT

    try {
        if (data_channel_.is_closed()) {
            buffer_.clear();
            return tateyama::status::ok;
        }
        if (!buffer_.empty()) {
            stream_.send_resultset(slot_, writer_id_, {buffer_.data(), buffer_.size()});
            buffer_.clear();
        } else {
            stream_.send_resultset(slot_, writer_id_, {});
        }
        return tateyama::status::ok;
    } catch (std::runtime_error &ex) {
        if (data_channel_.is_closed()) {
            buffer_.clear();
            return tateyama::status::ok;
        }
        return tateyama::status::unknown;
    }
}

}
