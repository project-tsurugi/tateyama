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
#pragma once

#include <set>
#include <string_view>
#include <mutex>
#include <atomic>
#include <functional>

#include "tateyama/endpoint/common/response.h"
#include "tateyama/endpoint/common/pointer_comp.h"
#include "server_wires.h"
#include "ipc_request.h"

namespace tateyama::endpoint::ipc {

constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_BODY = 1;
constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_BODYHEAD = 2;
constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_CODE = 3;

class ipc_data_channel;
class ipc_response;

/**
 * @brief writer object for ipc_endpoint
 */
class alignas(64) ipc_writer : public tateyama::api::server::writer {
    friend ipc_data_channel;

public:
    explicit ipc_writer(server_wire_container::unq_p_resultset_wire_conteiner wire) : resultset_wire_(std::move(wire)) {}

    tateyama::status write(char const* data, std::size_t length) override;
    tateyama::status commit() override;
    void release();

private:
    server_wire_container::unq_p_resultset_wire_conteiner resultset_wire_;
};

/**
 * @brief data_channel object for ipc_endpoint
 */
class alignas(64) ipc_data_channel : public tateyama::api::server::data_channel {
public:
    explicit ipc_data_channel(server_wire_container::unq_p_resultset_wires_conteiner data_channel)
        : data_channel_(std::move(data_channel)) {
    }

    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) override;
    tateyama::status release(tateyama::api::server::writer& wrt) override;
    void set_eor() { return data_channel_->set_eor(); }
    bool is_closed() { return data_channel_->is_closed(); }
    server_wire_container::unq_p_resultset_wires_conteiner get_resultset_wires() { return std::move(data_channel_); }

private:
    server_wire_container::unq_p_resultset_wires_conteiner data_channel_;

    std::set<std::shared_ptr<ipc_writer>, tateyama::endpoint::common::pointer_comp<ipc_writer>> data_writers_{};
    std::mutex mutex_{};
};

/**
 * @brief response object for ipc_endpoint
 */
class alignas(64) ipc_response : public tateyama::endpoint::common::response {
    friend ipc_data_channel;

public:
    ipc_response(std::shared_ptr<server_wire_container> server_wire, std::size_t index, std::function<void(void)> clean_up) :
        server_wire_(std::move(server_wire)),
        index_(index),
        garbage_collector_(server_wire_->get_garbage_collector()),
        clean_up_(std::move(clean_up)) {
        // do dump here
        garbage_collector_->dump();
    }
    ipc_response(std::shared_ptr<server_wire_container> server_wire, std::size_t index) :
        ipc_response(std::move(server_wire), index, [](){}) {
    }
    ipc_response() = delete;
    ~ipc_response() override {
        clean_up_();
    }

    ipc_response(ipc_response const&) = delete;
    ipc_response(ipc_response&&) = delete;
    ipc_response& operator = (ipc_response const&) = delete;
    ipc_response& operator = (ipc_response&&) = delete;

    tateyama::status body(std::string_view body) override;
    tateyama::status body_head(std::string_view body_head) override;
    void error(proto::diagnostics::Record const& record) override;
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) override;
    tateyama::status release_channel(tateyama::api::server::data_channel& ch) override;

    void session_id(std::size_t id) override {
        session_id_ = id;
    }

private:
    std::shared_ptr<server_wire_container> server_wire_;
    std::size_t index_;
    garbage_collector* garbage_collector_;
    const std::function<void(void)> clean_up_;

    std::string message_{};

    std::shared_ptr<ipc_data_channel> data_channel_{};

    std::size_t session_id_{};

    std::atomic_flag completed_{};

    void server_diagnostics(std::string_view diagnostic_record);
};

}  // tateyama::common::wire
