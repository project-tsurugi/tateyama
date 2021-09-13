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

#include <glog/logging.h>

#include "tateyama/endpoint/ipc/wire.h"

#include "tateyama/endpoint/ipc/server_wires.h"

namespace tsubakuro::common::wire {

class server_wire_container_impl : public server_wire_container
{
    static constexpr std::size_t shm_size = (1<<22);  // 4M bytes (tentative)
    static constexpr std::size_t request_buffer_size = (1<<12);   //  4K bytes (tentative)
    static constexpr std::size_t resultset_vector_size = (1<<12); //  4K bytes (tentative)
    static constexpr std::size_t writer_count = 8;

public:
    // resultset_wires_container
    class resultset_wires_container_impl : public resultset_wires_container {
    public:
        //   for server
        resultset_wires_container_impl(boost::interprocess::managed_shared_memory* managed_shm_ptr, std::string_view name, std::size_t count)
            : managed_shm_ptr_(managed_shm_ptr), rsw_name_(name), server_(true) {
            managed_shm_ptr_->destroy<shm_resultset_wires>(rsw_name_.c_str());
            shm_resultset_wires_ = managed_shm_ptr_->construct<shm_resultset_wires>(rsw_name_.c_str())(managed_shm_ptr_, count);
        }
        //   for client (test purpose)
        resultset_wires_container_impl(boost::interprocess::managed_shared_memory* managed_shm_ptr, std::string_view name)
            : managed_shm_ptr_(managed_shm_ptr), rsw_name_(name), server_(false) {
            shm_resultset_wires_ = managed_shm_ptr_->find<shm_resultset_wires>(rsw_name_.c_str()).first;
            if (shm_resultset_wires_ == nullptr) {
                throw std::runtime_error("cannot find the resultset wire");
            }
        }
        ~resultset_wires_container_impl() {
            if (server_) {
                managed_shm_ptr_->destroy<shm_resultset_wires>(rsw_name_.c_str());
            }
        }

        shm_resultset_wire* acquire() {
            try {
                return shm_resultset_wires_->acquire();
            }
            catch(const boost::interprocess::interprocess_exception& ex) {
                LOG(FATAL) << "running out of boost managed shared memory" << std::endl;
                pthread_exit(nullptr);  // FIXME
            }
        }

        std::pair<char*, std::size_t> get_chunk(bool wait_flag = false) {
            if (current_wire_ == nullptr) {
                current_wire_ = search();
            }
            if (current_wire_ != nullptr) {
                return current_wire_->get_chunk(current_wire_->get_bip_address(managed_shm_ptr_), wait_flag);
            }
            std::abort();  //  FIXME
        }
        void dispose(std::size_t length) {
            if (current_wire_ != nullptr) {
                current_wire_->dispose(current_wire_->get_bip_address(managed_shm_ptr_), length);
                current_wire_ = nullptr;
                return;
            }
            std::abort();  //  FIXME
        }
        bool is_eor() {
            if (current_wire_ == nullptr) {
                current_wire_ = search();
            }
            if (current_wire_ != nullptr) {
                auto rv = current_wire_->is_eor();
                current_wire_ = nullptr;
                return rv;
            }
            std::abort();  //  FIXME
        }
        bool is_closed() {
            return shm_resultset_wires_->is_closed();
        }

    private:
        shm_resultset_wire* search() {
            return shm_resultset_wires_->search();
        }

        boost::interprocess::managed_shared_memory* managed_shm_ptr_;
        std::string rsw_name_;
        shm_resultset_wires* shm_resultset_wires_{};
        //   for client
        shm_resultset_wire* current_wire_{};
        bool server_;
    };
    static void resultset_deleter_impl(resultset_wires_container* resultset) {
        delete static_cast<resultset_wires_container_impl*>(resultset);
    }

    class garbage_collector_impl : public garbage_collector
    {
    public:
        garbage_collector_impl() {}
        ~garbage_collector_impl() {
            resultset_wires_set_.clear();
        }

        void put(unq_p_resultset_wires_conteiner wires) {
            resultset_wires_set_.emplace(std::move(wires));
        }
        void dump() {
            std::set<unq_p_resultset_wires_conteiner>::iterator it = resultset_wires_set_.begin();
            while (it != resultset_wires_set_.end()) {
                if ((*it)->is_closed()) {
                    resultset_wires_set_.erase(it++);
                } else {
                    it++;
                }
            }
        }

    private:
        std::set<unq_p_resultset_wires_conteiner> resultset_wires_set_{};
    };


    class wire_container_impl : public wire_container {
    public:
        wire_container_impl() = default;
        wire_container_impl(unidirectional_message_wire* wire, char* bip_buffer) : wire_(wire), bip_buffer_(bip_buffer) {};
        message_header peep(bool wait = false) {
            return wire_->peep(bip_buffer_, wait);
        }
        const char* payload(std::size_t length) {
            return wire_->payload(bip_buffer_, length);
        }
        void write(const char* from, message_header&& header) {
            wire_->write(bip_buffer_, from, std::move(header));
        }
        void read(char* to, std::size_t msg_len) {
            wire_->read(to, bip_buffer_, msg_len);
        }
        std::size_t read_point() { return wire_->read_point(); }
        void dispose(const std::size_t rp) { wire_->dispose(bip_buffer_, rp); }

    private:
        unidirectional_message_wire* wire_;
        char* bip_buffer_;
    };

    server_wire_container_impl(std::string_view name) : name_(name), garbage_collector_impl_(std::make_unique<garbage_collector_impl>()) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), shm_size);
            auto req_wire = managed_shared_memory_->construct<unidirectional_message_wire>(request_wire_name)(managed_shared_memory_.get(), request_buffer_size);
            request_wire_ = wire_container_impl(req_wire, req_wire->get_bip_address(managed_shared_memory_.get()));
            responses_ = managed_shared_memory_->construct<response_box>(response_box_name)(16, managed_shared_memory_.get());
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            LOG(FATAL) << "running out of boost managed shared memory" << std::endl;
            pthread_exit(nullptr);  // FIXME
        }
    }

    /**
     * @brief Copy and move constructers are deleted.
     */
    server_wire_container_impl(server_wire_container_impl const&) = delete;
    server_wire_container_impl(server_wire_container_impl&&) = delete;
    server_wire_container_impl& operator = (server_wire_container_impl const&) = delete;
    server_wire_container_impl& operator = (server_wire_container_impl&&) = delete;

    ~server_wire_container_impl() {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
    }

    wire_container* get_request_wire() { return &request_wire_; }
    response_box::response& get_response(std::size_t idx) { return responses_->at(idx); }

    unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view name, std::size_t count) {
        try {
            return std::unique_ptr<resultset_wires_container_impl, resultset_deleter_type>{
                new resultset_wires_container_impl{managed_shared_memory_.get(), name, count}, resultset_deleter_impl };
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            LOG(FATAL) << "running out of boost managed shared memory" << std::endl;
            pthread_exit(nullptr);  // FIXME
        }
    }
    unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view name) {
        return create_resultset_wires(name, writer_count);
    }
    garbage_collector* get_garbage_collector() {
        return garbage_collector_impl_.get();
    }
    void close_session() { session_closed_ = true; }

    std::unique_ptr<resultset_wires_container_impl> create_resultset_wires_for_client(std::string_view name) {
        return std::make_unique<resultset_wires_container_impl>(managed_shared_memory_.get(), name);
    }
    bool is_session_closed() { return session_closed_; }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    wire_container_impl request_wire_;
    response_box* responses_;
    std::unique_ptr<garbage_collector_impl> garbage_collector_impl_;
    bool session_closed_{false};
};

class connection_container
{
    static constexpr std::size_t request_queue_size = (1<<12);  // 4K bytes (tentative)

public:
    connection_container(std::string_view name) : name_(name) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), request_queue_size);
            managed_shared_memory_->destroy<connection_queue>(connection_queue::name);
            connection_queue_ = managed_shared_memory_->construct<connection_queue>(connection_queue::name)();
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            std::abort();  // FIXME
        }
    }

    /**
     * @brief Copy and move constructers are deleted.
     */
    connection_container(connection_container const&) = delete;
    connection_container(connection_container&&) = delete;
    connection_container& operator = (connection_container const&) = delete;
    connection_container& operator = (connection_container&&) = delete;

    ~connection_container() {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
    }
    connection_queue& get_connection_queue() {
        return *connection_queue_;
    }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    connection_queue* connection_queue_;
};

};  // namespace tsubakuro::common
