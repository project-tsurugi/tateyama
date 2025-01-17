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

#include "ipc_response.h"
#include "tateyama/endpoint/common/endpoint_proto_utils.h"

namespace tateyama::endpoint::ipc {

// class server_wire
tateyama::status ipc_response::body(std::string_view body) {
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true)) {
        VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " length = " << body.length() << " slot = " << index_;  //NOLINT
        if (data_channel_) {
            std::dynamic_pointer_cast<ipc_data_channel>(data_channel_)->shutdown();  // Guard against improper operation
        }
        clean_up_();

        std::stringstream ss{};
        endpoint::common::header_content arg{};
        arg.session_id_ = session_id_;
        if(auto res = endpoint::common::append_response_header(ss, body, arg, ::tateyama::proto::framework::response::Header::SERVICE_RESULT); ! res) {
            LOG_LP(ERROR) << "error formatting response message";
            return status::unknown;
        }
        auto s = ss.str();
        server_wire_->get_response_wire().write(s.data(), tateyama::common::wire::response_header(index_, s.length(), RESPONSE_BODY));
        return tateyama::status::ok;
    }
    LOG_LP(ERROR) << "response is already completed";
    return status::unknown;        
}

tateyama::status ipc_response::body_head(std::string_view body_head) {
    if (check_cancel()) {
        LOG_LP(ERROR) << "request correspoinding to the response has been canceled";
        return status::unknown;
    }
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " slot = " << index_;  //NOLINT

    std::stringstream ss{};
    endpoint::common::header_content arg{};
    arg.session_id_ = session_id_;
    if(auto res = endpoint::common::append_response_header(ss, body_head, arg, ::tateyama::proto::framework::response::Header::SERVICE_RESULT); ! res) {
        LOG_LP(ERROR) << "error formatting response message";
        return status::unknown;
    }
    auto s = ss.str();
    server_wire_->get_response_wire().write(s.data(), tateyama::common::wire::response_header(index_, s.length(), RESPONSE_BODYHEAD));
    set_state(state::to_be_used);
    return tateyama::status::ok;
}

void ipc_response::error(proto::diagnostics::Record const& record) {
    bool expected = false;
    if (completed_.compare_exchange_strong(expected, true)) {
        VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " slot = " << index_;  //NOLINT
        if (data_channel_) {
            std::dynamic_pointer_cast<ipc_data_channel>(data_channel_)->shutdown();  // Guard against improper operation
        }
        clean_up_();

        std::string s{};
        if(record.SerializeToString(&s)) {
            server_diagnostics(s);
        } else {
            LOG_LP(ERROR) << "error formatting diagnostics message";
            server_diagnostics("");
        }
    } else {
        LOG_LP(ERROR) << "response is already completed";
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
    server_wire_->get_response_wire().write(s.data(), tateyama::common::wire::response_header(index_, s.length(), RESPONSE_BODY));
}

tateyama::status ipc_response::acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch, std::size_t writer_count) {
    if (completed_.load()) {
        LOG_LP(ERROR) << "response is already completed";
        set_state(state::acquire_failed);
        return tateyama::status::unknown;
    }
    if (writer_count > (UINT8_MAX + 1)) {
        LOG_LP(ERROR) << "too large writer count (" << writer_count << ") given";
        set_state(state::acquire_failed);
        return tateyama::status::unknown;
    }
    try {
        data_channel_ = std::make_shared<ipc_data_channel>(server_wire_->create_resultset_wires(name, writer_count), garbage_collector_);
    } catch (std::exception &ex) {
        ch = nullptr;

        LOG_LP(INFO) << "Running out of shared memory for result set transfers. Probably due to too many result sets being opened";
        set_state(state::acquire_failed);
        return tateyama::status::unknown;
    }
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    if (ch = data_channel_; ch != nullptr) {
        set_state(state::acquired);
        return tateyama::status::ok;
    }
    LOG_LP(ERROR) << "cannot aquire a channel for unknown reason";
    set_state(state::acquire_failed);
    return tateyama::status::unknown;
}

tateyama::status ipc_response::release_channel(tateyama::api::server::data_channel& ch) {
    VLOG_LP(log_trace) << static_cast<const void*>(server_wire_.get()) << " data_channel_ = " << static_cast<const void*>(data_channel_.get());  //NOLINT

    if (data_channel_.get() == &ch) {
        std::dynamic_pointer_cast<ipc_data_channel>(data_channel_)->shutdown();
        data_channel_ = nullptr;
        set_state(state::released);
        return tateyama::status::ok;
    }
    LOG_LP(ERROR) << "the parameter given (tateyama::api::server::data_channel& ch) does not match the data_channel_ this object has";
    set_state(state::release_failed);
    return tateyama::status::unknown;
}

// class ipc_data_channel
tateyama::status ipc_data_channel::acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) {
    try {
        auto ipc_wrt = std::make_shared<ipc_writer>(resultset_wires_->acquire());
        wrt = std::dynamic_pointer_cast<tateyama::api::server::writer>(ipc_wrt);
        VLOG_LP(log_trace) << " data_channel_ = " << static_cast<const void*>(this) << " writer = " << static_cast<const void*>(wrt.get());  //NOLINT
        {
            std::unique_lock lock{mutex_};
            data_writers_.emplace(std::move(ipc_wrt));
        }
        return tateyama::status::ok;
    } catch(const std::runtime_error& ex) {
        wrt = nullptr;

        LOG_LP(INFO) << ex.what();
        return tateyama::status::unknown;
    } catch (const std::exception &ex) {
        wrt = nullptr;

        LOG_LP(ERROR) << "unknown error ipc_data_channel::acquire: " << ex.what();
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
    LOG_LP(ERROR) << "cannot find the writer to be released";
    return tateyama::status::unknown;
}

void ipc_data_channel::shutdown() {
    std::unique_lock lock{mutex_};

    if (resultset_wires_) {
        resultset_wires_->set_eor();
        for (auto&& it = data_writers_.begin(), last = data_writers_.end(); it != last;) {
            (*it)->release();
            it = data_writers_.erase(it);
        }
        if (!resultset_wires_->is_closed()) {
            garbage_collector_.put(std::move(resultset_wires_));
        } else {
            resultset_wires_ = nullptr;
        }
    }
}

// class writer
tateyama::status ipc_writer::write(char const* data, std::size_t length) {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT
    if (released_.load()) {
        LOG_LP(INFO) << "ipc_writer (" << static_cast<const void*>(this) << ") has already been released";  //NOLINT
        return tateyama::status::unknown;
    }

    try {
        resultset_wire_->write(data, length);
        return tateyama::status::ok;
    } catch (std::exception &ex) {
        LOG_LP(ERROR) << ex.what();
    }
    return tateyama::status::unknown;
}

tateyama::status ipc_writer::commit() {
    VLOG_LP(log_trace) << static_cast<const void*>(this);  //NOLINT
    if (released_.load()) {
        LOG_LP(INFO) << "ipc_writer (" << static_cast<const void*>(this) << ") has already been released";  //NOLINT
        return tateyama::status::unknown;
    }

    resultset_wire_->flush();
    return tateyama::status::ok;
}

void ipc_writer::release() {
    released_.store(true);
    resultset_wire_->release(std::move(resultset_wire_));
}

}
