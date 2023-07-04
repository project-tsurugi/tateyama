/*
 * Copyright 2019-2021 tsurugi project.
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

#include <tateyama/api/server/response.h>
#include <tateyama/api/server/response_code.h>

#include "server_wires.h"
#include "ipc_request.h"

namespace tateyama::common::wire {

template<class T>
struct pointer_comp {
    using is_transparent = std::true_type;
    // helper does some magic in order to reduce the number of
    // pairs of types we need to know how to compare: it turns
    // everything into a pointer, and then uses `std::less<T*>`
    // to do the comparison:
    class helper {
        T* ptr;
    public:
        helper():ptr(nullptr) {}
        helper(helper const&) = default;
        helper(helper&&) noexcept = default;
        helper& operator = (helper const&) = default;
        helper& operator = (helper&&) noexcept = default;
        helper(T* const p):ptr(p) {}  //NOLINT
        template<class U>
        helper( std::shared_ptr<U> const& sp ):ptr(sp.get()) {}  //NOLINT
        template<class U, class...Ts>
        helper( std::unique_ptr<U, Ts...> const& up ):ptr(up.get()) {}  //NOLINT
        ~helper() = default;
        // && optional: enforces rvalue use only
        bool operator<( helper const o ) const {
            return std::less<T*>()( ptr, o.ptr );
        }
    };
    // without helper, we would need 2^n different overloads, where
    // n is the number of types we want to support (so, 8 with
    // raw pointers, unique pointers, and shared pointers).  That
    // seems silly:
    // && helps enforce rvalue use only
    bool operator()( helper const&& lhs, helper const&& rhs ) const {
        return lhs < rhs;
    }
};

constexpr static response_header::msg_type RESPONSE_BODY = 1;
constexpr static response_header::msg_type RESPONSE_BODYHEAD = 2;
constexpr static response_header::msg_type RESPONSE_CODE = 3;

class ipc_data_channel;

/**
 * @brief writer object for ipc_endpoint
 */
class ipc_writer : public tateyama::api::server::writer {
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
class ipc_data_channel : public tateyama::api::server::data_channel {
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

    std::set<std::shared_ptr<ipc_writer>, pointer_comp<ipc_writer>> data_writers_{};
    std::mutex mutex_{};
};

/**
 * @brief response object for ipc_endpoint
 */
class ipc_response : public tateyama::api::server::response {
public:
    ipc_response(ipc_request& request, std::size_t index) :
        ipc_request_(request),
        server_wire_(ipc_request_.get_server_wire_container()),
        index_(index),
        garbage_collector_(server_wire_.get_garbage_collector()) {
        // do dump here
        garbage_collector_->dump();
    }

    ipc_response() = delete;

    void code(tateyama::api::server::response_code code) override;
    tateyama::status body(std::string_view body) override;
    tateyama::status body_head(std::string_view body_head) override;
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel>& ch) override;
    tateyama::status release_channel(tateyama::api::server::data_channel& ch) override;
    tateyama::status close_session() override;

    void session_id(std::size_t id) override {
        session_id_ = id;
    }
private:
    ipc_request& ipc_request_;
    server_wire_container& server_wire_;
    std::size_t index_;
    tateyama::common::wire::garbage_collector* garbage_collector_;

    tateyama::api::server::response_code response_code_{tateyama::api::server::response_code::unknown};
    std::string message_{};

    std::shared_ptr<ipc_data_channel> data_channel_{};

    std::size_t session_id_{};

    void server_diagnostics(std::string_view diagnostic_record);
};

}  // tateyama::common::wire
