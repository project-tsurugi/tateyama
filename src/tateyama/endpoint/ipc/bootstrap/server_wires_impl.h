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
#include <mutex>

#include <glog/logging.h>

#include <tateyama/endpoint/ipc/wire.h>
#include <tateyama/endpoint/ipc/server_wires.h>

namespace tateyama::common::wire {

class server_wire_container_impl : public server_wire_container
{
    static constexpr std::size_t shm_size = (1<<21) * 104;  // 2M(64K * 8writes * 4result_sets) * 104threads bytes (tentative)  NOLINT
    static constexpr std::size_t request_buffer_size = (1<<12);   //  4K bytes (tentative)  NOLINT
    static constexpr std::size_t response_buffer_size = (1<<14);  // 16K bytes (tentative)  NOLINT
    static constexpr std::size_t resultset_vector_size = (1<<12); //  4K bytes (tentative)  NOLINT
    static constexpr std::size_t writer_count = 16;

public:
    // resultset_wires_container
    class resultset_wires_container_impl : public resultset_wires_container {
    public:
        //   for server
        resultset_wires_container_impl(boost::interprocess::managed_shared_memory* managed_shm_ptr, std::string_view name, std::size_t count, std::mutex& mtx_shm)
            : managed_shm_ptr_(managed_shm_ptr), rsw_name_(name), server_(true), mtx_shm_(mtx_shm) {
            std::lock_guard<std::mutex> lock(mtx_shm_);
            managed_shm_ptr_->destroy<shm_resultset_wires>(rsw_name_.c_str());
            shm_resultset_wires_ = managed_shm_ptr_->construct<shm_resultset_wires>(rsw_name_.c_str())(managed_shm_ptr_, count);
        }
        ~resultset_wires_container_impl() override {
            if (server_) {
                std::lock_guard<std::mutex> lock(mtx_shm_);
                managed_shm_ptr_->destroy<shm_resultset_wires>(rsw_name_.c_str());
            }
        }

        /**
         * @brief Copy and move constructers are delete.
         */
        resultset_wires_container_impl(resultset_wires_container_impl const&) = delete;
        resultset_wires_container_impl(resultset_wires_container_impl&&) = delete;
        resultset_wires_container_impl& operator = (resultset_wires_container_impl const&) = delete;
        resultset_wires_container_impl& operator = (resultset_wires_container_impl&&) = delete;

        shm_resultset_wire* acquire() override {
            try {
                return shm_resultset_wires_->acquire();
            }
            catch(const boost::interprocess::interprocess_exception& ex) {
                LOG(ERROR) << "running out of boost managed shared memory" << std::endl;
                pthread_exit(nullptr);  // FIXME
            }
        }

        void set_eor() override {
            shm_resultset_wires_->set_eor();
        }
        bool is_closed() override {
            return shm_resultset_wires_->is_closed();
        }

    private:
        boost::interprocess::managed_shared_memory* managed_shm_ptr_;
        std::string rsw_name_;
        shm_resultset_wires* shm_resultset_wires_{};
        bool server_;
        std::mutex& mtx_shm_;
    };
    static void resultset_deleter_impl(resultset_wires_container* resultset) {
        delete dynamic_cast<resultset_wires_container_impl*>(resultset);  // NOLINT
    }

    class garbage_collector_impl : public garbage_collector
    {
    public:
        garbage_collector_impl() = default;
        ~garbage_collector_impl() override {
            resultset_wires_set_.clear();
        }

        /**
         * @brief Copy and move constructers are delete.
         */
        garbage_collector_impl(garbage_collector_impl const&) = delete;
        garbage_collector_impl(garbage_collector_impl&&) = delete;
        garbage_collector_impl& operator = (garbage_collector_impl const&) = delete;
        garbage_collector_impl& operator = (garbage_collector_impl&&) = delete;

        void put(unq_p_resultset_wires_conteiner wires) override {
            std::lock_guard<std::mutex> lock(mtx_put_);
            resultset_wires_set_.emplace(std::move(wires));
        }
        void dump() override {
            if (mtx_dump_.try_lock()) {
                std::lock_guard<std::mutex> lock(mtx_put_);

                auto it = resultset_wires_set_.begin();
                while (it != resultset_wires_set_.end()) {
                    if ((*it)->is_closed()) {
                        resultset_wires_set_.erase(it++);
                    } else {
                        it++;
                    }
                }
                mtx_dump_.unlock();
            }
        }

    private:
        std::set<unq_p_resultset_wires_conteiner> resultset_wires_set_{};
        std::mutex mtx_put_{};
        std::mutex mtx_dump_{};
    };


    class wire_container_impl : public wire_container {
    public:
        wire_container_impl() = default;
        ~wire_container_impl() override = default;
        wire_container_impl(wire_container_impl const&) = delete;
        wire_container_impl(wire_container_impl&&) = delete;
        wire_container_impl& operator = (wire_container_impl const&) = delete;
        wire_container_impl& operator = (wire_container_impl&&) = delete;

        void initialize(unidirectional_message_wire* wire, char* bip_buffer) {
            wire_ = wire;
            bip_buffer_ = bip_buffer;
        }
        message_header peep(bool wait = false) {
            return wire_->peep(bip_buffer_, wait);
        }
        std::string_view payload() override {
            return wire_->payload(bip_buffer_);
        }
        void read(char* to) override {
            wire_->read(to, bip_buffer_);
        }
        std::size_t read_point() override { return wire_->read_point(); }
        void dispose(const std::size_t rp) override { wire_->dispose(bip_buffer_, rp); }

    private:
        unidirectional_message_wire* wire_{};
        char* bip_buffer_{};
    };

    class response_wire_container_impl : public response_wire_container {
    public:
        response_wire_container_impl() = default;
        ~response_wire_container_impl() override = default;
        response_wire_container_impl(response_wire_container_impl const&) = delete;
        response_wire_container_impl(response_wire_container_impl&&) = delete;
        response_wire_container_impl& operator = (response_wire_container_impl const&) = delete;
        response_wire_container_impl& operator = (response_wire_container_impl&&) = delete;

        void initialize(unidirectional_response_wire* wire, char* bip_buffer) {
            wire_ = wire;
            bip_buffer_ = bip_buffer;
        }
        void write(const char* from, response_header header) override {
            std::lock_guard<std::mutex> lock(mtx_);
            wire_->write(bip_buffer_, from, header);
        }

    private:
        unidirectional_response_wire* wire_{};
        char* bip_buffer_{};
        std::mutex mtx_{};
    };

    explicit server_wire_container_impl(std::string_view name) : name_(name), garbage_collector_impl_(std::make_unique<garbage_collector_impl>()) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), shm_size);
            auto req_wire = managed_shared_memory_->construct<unidirectional_message_wire>(request_wire_name)(managed_shared_memory_.get(), request_buffer_size);
            auto res_wire = managed_shared_memory_->construct<unidirectional_response_wire>(response_wire_name)(managed_shared_memory_.get(), response_buffer_size);

            request_wire_.initialize(req_wire, req_wire->get_bip_address(managed_shared_memory_.get()));
            response_wire_.initialize(res_wire, res_wire->get_bip_address(managed_shared_memory_.get()));
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            LOG(ERROR) << "running out of boost managed shared memory" << std::endl;
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

    ~server_wire_container_impl() override {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
    }

    wire_container* get_request_wire() override { return &request_wire_; }
    response_wire_container& get_response_wire() override { return response_wire_; }

    unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view name, std::size_t count) override {
        try {
            return std::unique_ptr<resultset_wires_container_impl, resultset_deleter_type>{
                new resultset_wires_container_impl{managed_shared_memory_.get(), name, count, mtx_shm_}, resultset_deleter_impl };
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            LOG(ERROR) << "running out of boost managed shared memory" << std::endl;
            pthread_exit(nullptr);  // FIXME
        }
    }
    unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view name) override {
        return create_resultset_wires(name, writer_count);
    }
    garbage_collector* get_garbage_collector() override {
        return garbage_collector_impl_.get();
    }
    void close_session() override { session_closed_ = true; }

    [[nodiscard]] bool is_session_closed() const { return session_closed_; }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    wire_container_impl request_wire_{};
    response_wire_container_impl response_wire_{};
    std::unique_ptr<garbage_collector_impl> garbage_collector_impl_;
    bool session_closed_{false};
    std::mutex mtx_shm_{};
};


class connection_container
{
    static constexpr std::size_t request_queue_size = (1<<12);  // 4K bytes (tentative)  NOLINT

public:
    explicit connection_container(std::string_view name) : name_(name) {
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

};  // namespace tateyama::common::wire
