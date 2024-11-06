/*
 * Copyright 2018-2024 Project Tsurugi.
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
    std::atomic_bool released_{};
};

/**
 * @brief data_channel object for ipc_endpoint
 */
class alignas(64) ipc_data_channel : public tateyama::api::server::data_channel {
    friend ipc_response;

public:
    explicit ipc_data_channel(server_wire_container::unq_p_resultset_wires_conteiner resultset_wires)
        : resultset_wires_(std::move(resultset_wires)) {
    }
    ~ipc_data_channel() override {
        std::unique_lock lock{mutex_};
        resultset_wires_ = nullptr;
    }

    ipc_data_channel(ipc_data_channel const&) = delete;
    ipc_data_channel(ipc_data_channel&&) = delete;
    ipc_data_channel& operator = (ipc_data_channel const&) = delete;
    ipc_data_channel& operator = (ipc_data_channel&&) = delete;

    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer>& wrt) override;
    tateyama::status release(tateyama::api::server::writer& wrt) override;
    void set_eor() { return resultset_wires_->set_eor(); }
    bool is_closed() { return resultset_wires_->is_closed(); }
    server_wire_container::unq_p_resultset_wires_conteiner::pointer resultset_wires_conteiner() {
        return resultset_wires_.get();
    }

private:
    server_wire_container::unq_p_resultset_wires_conteiner resultset_wires_;

    std::set<std::shared_ptr<ipc_writer>, tateyama::endpoint::common::pointer_comp<ipc_writer>> data_writers_{};
    std::mutex mutex_{};

    void release();
};

/**
 * @brief response object for ipc_endpoint
 */
class alignas(64) ipc_response : public tateyama::endpoint::common::response {
    friend ipc_data_channel;

public:
    static constexpr std::size_t default_writer_count = 32;

    ipc_response(std::shared_ptr<server_wire_container> server_wire, std::size_t index, std::size_t writer_count, std::function<void(void)> clean_up) :
        tateyama::endpoint::common::response(index),
        server_wire_(std::move(server_wire)),
        garbage_collector_(*server_wire_->get_garbage_collector()),
        writer_count_(writer_count),
        clean_up_(std::move(clean_up)) {
    }
    ~ipc_response() override {
        data_channel_ = nullptr;
    }

    /**
     * @brief Copy and move constructers are delete.
     */
    ipc_response(ipc_response const&) = delete;
    ipc_response(ipc_response&&) = delete;
    ipc_response& operator = (ipc_response const&) = delete;
    ipc_response& operator = (ipc_response&&) = delete;

    tateyama::status body(std::string_view body) override;
    tateyama::status body_head(std::string_view body_head) override;
    void error(proto::diagnostics::Record const& record) override;
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) override;
    tateyama::status release_channel(tateyama::api::server::data_channel& ch) override;

private:
    std::shared_ptr<server_wire_container> server_wire_;
    garbage_collector& garbage_collector_;
    std::size_t writer_count_;
    const std::function<void(void)> clean_up_;

    void server_diagnostics(std::string_view diagnostic_record);
};

}  // tateyama::common::wire
