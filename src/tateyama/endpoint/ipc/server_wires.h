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

#include "wire.h"
#include "tateyama/logging_helper.h"

namespace tateyama::common::wire {

class server_wire_container
{
public:
    class wire_container {
    public:
        wire_container() = default;
        virtual ~wire_container() = 0;
        constexpr wire_container(wire_container const&) = delete;
        constexpr wire_container(wire_container&&) = delete;
        wire_container& operator = (wire_container const&) = default;
        wire_container& operator = (wire_container&&) = default;

        virtual std::size_t read_point() = 0;
        virtual std::string_view payload() = 0;
        virtual void read(char *) = 0;
        virtual void dispose() = 0;
    };
    class response_wire_container {
    public:
        response_wire_container() = default;
        virtual ~response_wire_container() = 0;
        constexpr response_wire_container(response_wire_container const&) = delete;
        constexpr response_wire_container(response_wire_container&&) = delete;
        response_wire_container& operator = (response_wire_container const&) = default;
        response_wire_container& operator = (response_wire_container&&) = default;

        virtual void write(const char*, response_header) = 0;
    };
    class resultset_wire_container;
    using resultset_wire_deleter_type = void(*)(resultset_wire_container*);
    using unq_p_resultset_wire_conteiner = std::unique_ptr<resultset_wire_container, resultset_wire_deleter_type>;
    class resultset_wire_container {
    public:
        resultset_wire_container() = default;
        virtual ~resultset_wire_container() = 0;
        constexpr resultset_wire_container(resultset_wire_container const&) = delete;
        constexpr resultset_wire_container(resultset_wire_container&&) = delete;
        resultset_wire_container& operator = (resultset_wire_container const&) = delete;
        resultset_wire_container& operator = (resultset_wire_container&&) = delete;

        virtual void write(char const*, std::size_t) = 0;
        virtual void flush() = 0;
        virtual void release(unq_p_resultset_wire_conteiner) = 0;
    };
    class resultset_wires_container {
    public:
        resultset_wires_container() = default;
        virtual ~resultset_wires_container() = 0;
        constexpr resultset_wires_container(resultset_wires_container const&) = delete;
        constexpr resultset_wires_container(resultset_wires_container&&) = delete;
        resultset_wires_container& operator = (resultset_wires_container const&) = delete;
        resultset_wires_container& operator = (resultset_wires_container&&) = delete;

        virtual unq_p_resultset_wire_conteiner acquire() = 0;
        virtual void set_eor() = 0;
        virtual bool is_closed() = 0;
    };
    using resultset_deleter_type = void(*)(resultset_wires_container*);
    using unq_p_resultset_wires_conteiner = std::unique_ptr<resultset_wires_container, resultset_deleter_type>;
    class garbage_collector {
    public:
        garbage_collector() = default;
        virtual ~garbage_collector() = 0;
        garbage_collector(garbage_collector const&) = delete;
        garbage_collector(garbage_collector&&) = delete;
        garbage_collector& operator = (garbage_collector const&) = delete;
        garbage_collector& operator = (garbage_collector&&) = delete;

        virtual void dump() = 0;
        virtual void put(unq_p_resultset_wires_conteiner) = 0;
    };

    server_wire_container() = default;
    virtual ~server_wire_container() = 0;
    constexpr server_wire_container(server_wire_container const&) = delete;
    constexpr server_wire_container(server_wire_container&&) = delete;
    server_wire_container& operator = (server_wire_container const&) = delete;
    server_wire_container& operator = (server_wire_container&&) = delete;

    virtual wire_container* get_request_wire() = 0;
    virtual response_wire_container& get_response_wire() = 0;
    virtual unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view) = 0;
    virtual unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view, std::size_t count) = 0;
    virtual garbage_collector* get_garbage_collector() = 0;
    virtual void close_session() = 0;
};
inline server_wire_container::~server_wire_container() = default;
inline server_wire_container::wire_container::~wire_container() = default;
inline server_wire_container::response_wire_container::~response_wire_container() = default;
inline server_wire_container::resultset_wire_container::~resultset_wire_container() = default;
inline server_wire_container::resultset_wires_container::~resultset_wires_container() = default;
inline server_wire_container::garbage_collector::~garbage_collector() = default;

using resultset_wires = server_wire_container::resultset_wires_container;
using resultset_wire = shm_resultset_wire;
using garbage_collector = server_wire_container::garbage_collector;

};  // namespace tateyama::common
