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

#include "wire.h"

namespace tsubakuro::common::wire {

class server_wire_container
{
public:
    class wire_container {
    public:
        virtual std::size_t read_point() = 0;
        virtual const char* payload(std::size_t) = 0;
        virtual void dispose(std::size_t) = 0;
    };
    class resultset_wires_container {
    public:
        virtual ~resultset_wires_container() = 0;
        virtual shm_resultset_wire* acquire() = 0;
        virtual bool is_closed() = 0;
    };
    using resultset_deleter_type = void(*)(resultset_wires_container*);
    using unq_p_resultset_wires_conteiner = std::unique_ptr<resultset_wires_container, resultset_deleter_type>;
    class garbage_collector {
    public:
        virtual void dump() = 0;
        virtual void put(unq_p_resultset_wires_conteiner) = 0;
    };
    virtual wire_container* get_request_wire() = 0;
    virtual response_box::response& get_response(std::size_t) = 0;
    virtual unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view) = 0;
    virtual unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view, std::size_t count) = 0;
    virtual garbage_collector* get_garbage_collector() = 0;
    virtual void close_session() = 0;
};
inline server_wire_container::resultset_wires_container::~resultset_wires_container() {};

using resultset_wires = server_wire_container::resultset_wires_container;
using resultset_wire = shm_resultset_wire;
using garbage_collector = server_wire_container::garbage_collector;

};  // namespace tsubakuro::common
