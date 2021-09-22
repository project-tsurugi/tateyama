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

#include <tateyama/api/endpoint/response.h>

#include "server_wires.h"
#include "ipc_request.h"

namespace tateyama::common::wire {

template<class T>
struct pointer_comp {
    typedef std::true_type is_transparent;
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

class ipc_data_channel;

/**
 * @brief writer object for ipc_endpoint
 */
class ipc_writer : public tateyama::api::endpoint::writer {
    friend ipc_data_channel;

public:
    explicit ipc_writer(resultset_wire* wire) : resultset_wire_(wire) {}

    tateyama::status write(char const* data, std::size_t length) override;
    tateyama::status commit() override;

private:
    resultset_wire* resultset_wire_;
};

/**
 * @brief data_channel object for ipc_endpoint
 */
class ipc_data_channel : public tateyama::api::endpoint::data_channel {
public:
    explicit ipc_data_channel(server_wire_container::unq_p_resultset_wires_conteiner data_channel)
        : data_channel_(std::move(data_channel)) {
    }

    tateyama::status acquire(tateyama::api::endpoint::writer*& wrt) override;
    tateyama::status release(tateyama::api::endpoint::writer& wrt) override;
    bool is_closed() { return data_channel_->is_closed(); }
    server_wire_container::unq_p_resultset_wires_conteiner get_resultset_wires() { return std::move(data_channel_); }

private:
    server_wire_container::unq_p_resultset_wires_conteiner data_channel_;

    std::set<std::unique_ptr<ipc_writer>, pointer_comp<ipc_writer>> data_writers_{};
};

/**
 * @brief response object for ipc_endpoint
 */
class ipc_response : public tateyama::api::endpoint::response {
public:
    ipc_response(ipc_request& request, std::size_t index) :
        ipc_request_(request),
        server_wire_(ipc_request_.get_server_wire_container()),
        response_box_(server_wire_.get_response(index)),
        garbage_collector_(server_wire_.get_garbage_collector()) {
        // do dump here
        garbage_collector_->dump();
    }

    ipc_response() = delete;

    void code(tateyama::api::endpoint::response_code code) override;
    void message(std::string_view msg) override;
    tateyama::status complete() override;
    tateyama::status body(std::string_view body) override;
    tateyama::status acquire_channel(std::string_view name, tateyama::api::endpoint::data_channel*& ch) override;
    tateyama::status release_channel(tateyama::api::endpoint::data_channel& ch) override;
    tateyama::status close_session() override;

private:
    ipc_request& ipc_request_;
    server_wire_container& server_wire_;
    response_box::response& response_box_;
    tateyama::common::wire::garbage_collector* garbage_collector_;

    tateyama::api::endpoint::response_code response_code_{tateyama::api::endpoint::response_code::started};
    std::string message_{};

    std::unique_ptr<ipc_data_channel> data_channel_{};
    bool acquire_channel_or_complete_{};
};

}  // tateyama::common::wire
