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
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <thread>
#include <sstream>
#include <string_view>
#include <functional>

#include <glog/logging.h>
#include <tateyama/logging.h>

#include "tateyama/endpoint/ipc/wire.h"
#include "tateyama/endpoint/ipc/server_wires.h"

namespace tateyama::endpoint::ipc::bootstrap {

class server_wire_container_impl : public server_wire_container
{
    static constexpr std::size_t request_buffer_size = (1<<12);   //  4K bytes NOLINT
    static constexpr std::size_t response_buffer_size = (1<<13);  //  8K bytes NOLINT
    static constexpr std::size_t data_channel_overhead = 7700;   //  by experiment NOLINT
    static constexpr std::size_t total_overhead = (1<<14);   //  16K bytes by experiment NOLINT

public:
    class resultset_wires_container_impl;

    // resultset_wire_container
    class resultset_wire_container_impl : public resultset_wire_container {
        class annex {
        public:
            explicit annex(std::size_t size) {
                buffer_.reserve(size);
                read_point_ = buffer_.cbegin();
            }
            bool is_room(std::size_t length) {
                std::unique_lock<std::mutex> lock(mtx_chunks_);
                if (buffer_.capacity() < (write_pos_ + chunk_size_ + length)) {
                    full_ = true;
                    cnd_chunks_.notify_one();
                    return false;
                }
                return true;
            }
            // There is an assumption that write() and flush() are not executed simultaneously
            void write(char const* data, std::size_t length) {
                buffer_.insert(write_pos_ + chunk_size_, data, length);
                chunk_size_ += length;
            }
            void flush() {
                std::unique_lock<std::mutex> lock(mtx_chunks_);
                if (chunk_size_ > 0) {
                    chunks_.push(chunk_size_);
                    cnd_chunks_.notify_one();
                }
                write_pos_ += chunk_size_;
                chunk_size_ = 0;
            }
            void notify() {
                std::unique_lock<std::mutex> lock(mtx_chunks_);
                cnd_chunks_.notify_one();
            }
            bool transfer(tateyama::common::wire::shm_resultset_wire* wire, bool& released) {
                while (!exhausted()) {
                    std::unique_lock<std::mutex> lock(mtx_chunks_);
                    if (chunks_.empty() && !full_ && !released) {
                        cnd_chunks_.wait(lock, [this, &released]{ return !chunks_.empty() || full_ || released; });
                    }
                    if (!chunks_.empty()) {
                        std::size_t record_size = chunks_.front();
                        chunks_.pop();
                        lock.unlock();
                        if (wire->wait_room(record_size)) {
                            return false;
                        }
                        wire->write(std::addressof(*read_point_), record_size);
                        wire->flush();
                        read_point_ += static_cast<std::int64_t>(record_size);
                        continue;
                    }
                    if (released && chunk_size_ == 0) {
                        return true;
                    }
                    if (full_) {
                        if (chunk_size_ > 0) {
                            if (wire->wait_room(chunk_size_)) {
                                return false;
                            }
                            wire->write(std::addressof(*read_point_), chunk_size_);
                            read_point_ += static_cast<std::int64_t>(chunk_size_);
                        }
                        return true;
                    }
                }
                return true;
            }
            bool exhausted() {
                return full_ && (read_point_ == buffer_.cend());
            }

        private:
            std::string buffer_{};
            std::string::const_iterator read_point_{};
            std::queue<std::size_t> chunks_{};
            std::size_t write_pos_{};
            std::size_t chunk_size_{};
            bool full_{};

            std::mutex mtx_chunks_{};
            std::condition_variable cnd_chunks_{};
        };

    public:
        resultset_wire_container_impl(tateyama::common::wire::shm_resultset_wire* resultset_wire, resultset_wires_container_impl& resultset_wires_container_impl, std::size_t datachannel_buffer_size)
            : shm_resultset_wire_(resultset_wire), resultset_wires_container_impl_(resultset_wires_container_impl), datachannel_buffer_size_(datachannel_buffer_size) {
            VLOG_LP(log_trace) << "creates a resultset_wire with a size of " << datachannel_buffer_size_ << " bytes";
        }
        ~resultset_wire_container_impl() override {
            if (thread_invoked_) {
                if(writer_thread_.joinable()) {
                    writer_thread_.join();
                }
            }
        }

        /**
         * @brief Copy and move constructers are delete.
         */
        resultset_wire_container_impl(resultset_wire_container_impl const&) = delete;
        resultset_wire_container_impl(resultset_wire_container_impl&&) = delete;
        resultset_wire_container_impl& operator = (resultset_wire_container_impl const&) = delete;
        resultset_wire_container_impl& operator = (resultset_wire_container_impl&&) = delete;

        void operator()() {
            while (true) {
                {
                    std::unique_lock<std::mutex> lock(mtx_queue_);

                    cnd_queue_.wait(lock, [this]{ return !queue_.empty() || released_; });
                    if (queue_.empty() && released_) {
                        VLOG_LP(log_trace) << "exit writer annex mode because the end of the records";
                        write_complete();
                        break;
                    }
                    auto annex = queue_.front().get();
                    lock.unlock();
                    if (auto rv = annex->transfer(shm_resultset_wire_, released_); !rv) {
                        VLOG_LP(log_trace) << "exit writer annex mode because the result set is closed by the client";
                        break;
                    }
                }
                {
                    std::unique_lock<std::mutex> lock(mtx_queue_);

                    queue_.pop();
                }
            }
            std::atomic_thread_fence(std::memory_order_acq_rel);
            thread_active_ = false;
        }
        void write(char const* data, std::size_t length) override {
            current_record_size += length;
            if ((current_record_size + tateyama::common::wire::length_header::size) > datachannel_buffer_size_) {
                throw std::runtime_error("too large record size");
            }
            if (!annex_mode_) {
                if (shm_resultset_wire_->check_room(length)) {
                    shm_resultset_wire_->write(data, length);
                    return;
                }
                VLOG_LP(log_trace) << "enter writer annex mode";
                annex_mode_ = true;
                queue_.emplace(std::make_unique<annex>(datachannel_buffer_size_));
                if (!thread_invoked_) {
                    thread_active_ = true;
                    writer_thread_ = std::thread(std::ref(*this));
                    std::atomic_thread_fence(std::memory_order_acq_rel);
                    thread_invoked_ = true;
                }
            }
            {
                std::unique_lock<std::mutex> lock(mtx_queue_);

                if (!queue_.empty()) {
                    auto* current_annex = queue_.back().get();
                    if (current_annex->is_room(length)) {
                        current_annex->write(data, length);
                        return;
                    }
                }
                VLOG_LP(log_trace) << "extend annex";
                queue_.emplace(std::make_unique<annex>(datachannel_buffer_size_));
                cnd_queue_.notify_one();
                auto* current_annex = queue_.back().get();
                current_annex->write(data, length);
            }
        }
        void flush() override {
            current_record_size = 0;
            if (!annex_mode_) {
                shm_resultset_wire_->flush();
                return;
            }
            {
                std::unique_lock<std::mutex> lock(mtx_queue_);

                if (!queue_.empty()) {
                    auto* current_annex = queue_.back().get();
                    current_annex->flush();
                }
            }
        }
        void release(unq_p_resultset_wire_conteiner resultset_wire) override;
        bool is_disposable() override { return !thread_active_; }

    private:
        tateyama::common::wire::shm_resultset_wire* shm_resultset_wire_;
        resultset_wires_container_impl &resultset_wires_container_impl_;

        std::queue<std::unique_ptr<annex>> queue_{};
        std::thread writer_thread_{};
        bool annex_mode_{};
        std::atomic_bool thread_invoked_{};
        std::atomic_bool thread_active_{};
        bool released_{};

        std::mutex mtx_queue_{};
        std::condition_variable cnd_queue_{};

        std::size_t datachannel_buffer_size_;
        std::size_t current_record_size{};

        void write_complete();
    };
    static void resultset_wire_deleter_impl(resultset_wire_container* resultset_wire) {
        delete dynamic_cast<resultset_wire_container_impl*>(resultset_wire);  // NOLINT
    }

    // resultset_wires_container
    class resultset_wires_container_impl : public resultset_wires_container {
    public:
        //   for server
        resultset_wires_container_impl(boost::interprocess::managed_shared_memory* managed_shm_ptr, std::string_view name, std::size_t count, std::mutex& mtx_shm, std::size_t datachannel_buffer_size)
            : managed_shm_ptr_(managed_shm_ptr), rsw_name_(name), server_(true), mtx_shm_(mtx_shm), datachannel_buffer_size_(datachannel_buffer_size) {
            std::lock_guard<std::mutex> lock(mtx_shm_);
            managed_shm_ptr_->destroy<tateyama::common::wire::shm_resultset_wires>(rsw_name_.c_str());
            try {
                shm_resultset_wires_ = managed_shm_ptr_->construct<tateyama::common::wire::shm_resultset_wires>(rsw_name_.c_str())(managed_shm_ptr_, count, datachannel_buffer_size_);
            } catch(const boost::interprocess::interprocess_exception& ex) {
                throw std::runtime_error(ex.what());
            } catch (std::exception &ex) {
                LOG_LP(ERROR) << "running out of boost managed shared memory";
                throw ex;
            }
        }
        //  constructor for client
        resultset_wires_container_impl(boost::interprocess::managed_shared_memory* managed_shm_ptr, std::string_view name, std::mutex& mtx_shm)
            : managed_shm_ptr_(managed_shm_ptr), rsw_name_(name), server_(false), mtx_shm_(mtx_shm), datachannel_buffer_size_(0) {
            shm_resultset_wires_ = managed_shm_ptr_->find<tateyama::common::wire::shm_resultset_wires>(rsw_name_.c_str()).first;
            if (shm_resultset_wires_ == nullptr) {
                throw std::runtime_error("cannot find the resultset wire");
            }
        }
        ~resultset_wires_container_impl() override {
            try {
                if (server_ && !expiration_time_over_) {
                    std::lock_guard<std::mutex> lock(mtx_shm_);
                    managed_shm_ptr_->destroy<tateyama::common::wire::shm_resultset_wires>(rsw_name_.c_str());
                }
            } catch (std::exception& e) {
                LOG_LP(WARNING) << e.what();
            }
        }

        /**
         * @brief Copy and move constructers are delete.
         */
        resultset_wires_container_impl(resultset_wires_container_impl const&) = delete;
        resultset_wires_container_impl(resultset_wires_container_impl&&) = delete;
        resultset_wires_container_impl& operator = (resultset_wires_container_impl const&) = delete;
        resultset_wires_container_impl& operator = (resultset_wires_container_impl&&) = delete;

        unq_p_resultset_wire_conteiner acquire() override {
            std::lock_guard<std::mutex> lock(mtx_shm_);
            try {
                writers_++;
                return std::unique_ptr<resultset_wire_container_impl, resultset_wire_deleter_type>{
                    new resultset_wire_container_impl{shm_resultset_wires_->acquire(), *this, datachannel_buffer_size_}, resultset_wire_deleter_impl};
            } catch(const boost::interprocess::interprocess_exception& ex) {
                throw std::runtime_error(ex.what());
            } catch(const std::runtime_error& ex) {
                throw ex;
            } catch (const std::exception &ex) {
                LOG_LP(ERROR) << "unknown error in resultset_wires_container_impl::acquire: " << ex.what();
                throw ex;
            }
        }

        void set_eor() override {
            eor_ = true;
            notify_eor_conditional();
        }
        bool is_closed() override {
            return shm_resultset_wires_->is_closed();
        }
        bool is_disposable() override {
            for (auto&& e : released_writers_) {
                if (!e->is_disposable()) {
                    return false;
                }
            }
            return true;
        }
        // in case lost client
        void expiration_time_over() override {
            shm_resultset_wires_->set_closed();
            expiration_time_over_ = true;
        }

        void add_released_writer(unq_p_resultset_wire_conteiner resultset_wire) {
            released_writers_.emplace(std::move(resultset_wire));
        }
        void write_complete() {
            completed_writers_++;
            notify_eor_conditional();
        }

        // for client
        std::string_view get_chunk() {
            if (wrap_around_.data()) {
                auto rv = wrap_around_;
                wrap_around_ = std::string_view();
                return rv;
            }
            if (current_wire_ == nullptr) {
                current_wire_ = active_wire();
            }
            if (current_wire_ != nullptr) {
                return current_wire_->get_chunk(current_wire_->get_bip_address(managed_shm_ptr_), wrap_around_);
            }
            return {};
        }
        void dispose(std::size_t) {
            if (current_wire_ != nullptr) {
                current_wire_->dispose(current_wire_->get_bip_address(managed_shm_ptr_));
                current_wire_ = nullptr;
                return;
            }
            std::abort();
        }
        bool is_eor() {
            return shm_resultset_wires_->is_eor();
        }

    private:
        boost::interprocess::managed_shared_memory* managed_shm_ptr_;
        std::string rsw_name_;
        tateyama::common::wire::shm_resultset_wires* shm_resultset_wires_{};
        bool server_;
        bool expiration_time_over_{};
        std::mutex& mtx_shm_;
        std::size_t datachannel_buffer_size_;

        std::set<unq_p_resultset_wire_conteiner> released_writers_{};
        std::atomic_ulong writers_{};
        std::atomic_ulong completed_writers_{};
        bool eor_{};
        std::atomic_flag notify_eor_{};
        
        void notify_eor_conditional() {
            if ((writers_.load() == completed_writers_.load()) && eor_) {
                if (!notify_eor_.test_and_set()) {
                    shm_resultset_wires_->set_eor();
                }
            }
        }

        //   for client
        std::string_view wrap_around_{};
        tateyama::common::wire::shm_resultset_wire* current_wire_{};

        tateyama::common::wire::shm_resultset_wire* active_wire() {
            return shm_resultset_wires_->active_wire();
        }
    };
    static void resultset_deleter_impl(resultset_wires_container* resultset) {
        delete dynamic_cast<resultset_wires_container_impl*>(resultset);  // NOLINT
    }

    class garbage_collector_impl : public garbage_collector
    {
    public:
        garbage_collector_impl() = default;
        ~garbage_collector_impl() override {
            std::lock_guard<std::mutex> lock(mtx_put_);
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
            if (expiration_time_over_) {
                wires->expiration_time_over();
            }
            resultset_wires_set_.emplace(std::move(wires));
        }
        void dump() override {
            if (!expiration_time_over_) {
                if (mtx_dump_.try_lock()) {
                    std::lock_guard<std::mutex> lock(mtx_put_);

                    auto it = resultset_wires_set_.begin();
                    while (it != resultset_wires_set_.end()) {
                        if ((*it)->is_closed() && (*it)->is_disposable()) {
                            resultset_wires_set_.erase(it++);
                        } else {
                            it++;
                        }
                    }
                    mtx_dump_.unlock();
                }
            }
        }
        bool empty() override {
            std::lock_guard<std::mutex> lock(mtx_put_);
            return resultset_wires_set_.empty() || expiration_time_over_;
        }
        void expiration_time_over() override {
            std::lock_guard<std::mutex> lock(mtx_put_);
            for (const auto& it : resultset_wires_set_) {
                it->expiration_time_over();
            }
            expiration_time_over_ = true;
        }

    private:
        std::set<unq_p_resultset_wires_conteiner> resultset_wires_set_{};
        bool expiration_time_over_{};
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

        void initialize(tateyama::common::wire::unidirectional_message_wire* wire, char* bip_buffer) {
            wire_ = wire;
            bip_buffer_ = bip_buffer;
        }
        tateyama::common::wire::message_header peep() {
            return wire_->peep(bip_buffer_);
        }
        std::string_view payload() override {
            return wire_->payload(bip_buffer_);
        }
        void read(char* to) override {
            wire_->read(to, bip_buffer_);
        }
        std::size_t read_point() override { return wire_->read_point(); }
        void dispose() override { wire_->dispose(); }
        void notify() override { wire_->notify(); }

        // for mainly client, except for terminate request from server
        void write(const char* from, const std::size_t len, tateyama::common::wire::message_header::index_type index) {
            wire_->write(bip_buffer_, from, tateyama::common::wire::message_header(index, len));
        }

    private:
        tateyama::common::wire::unidirectional_message_wire* wire_{};
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

        void initialize(tateyama::common::wire::unidirectional_response_wire* wire, char* bip_buffer) {
            wire_ = wire;
            bip_buffer_ = bip_buffer;
        }
        void write(const char* from, tateyama::common::wire::response_header header) override {
            std::lock_guard<std::mutex> lock(mtx_);
            wire_->write(bip_buffer_, from, header);
        }
        void notify_shutdown() override {
            wire_->notify_shutdown();
        }

        // for client
        tateyama::common::wire::response_header await() {
            return wire_->await(bip_buffer_);
        }
        [[nodiscard]] tateyama::common::wire::response_header::length_type get_length() const {
            return wire_->get_length();
        }
        [[nodiscard]] tateyama::common::wire::response_header::index_type get_idx() const {
            return wire_->get_idx();
        }
        [[nodiscard]] tateyama::common::wire::response_header::msg_type get_type() const {
            return wire_->get_type();
        }
        void read(char* to) {
            wire_->read(to, bip_buffer_);
        }
        void close() {
            wire_->close();
        }

    private:
        tateyama::common::wire::unidirectional_response_wire* wire_{};
        char* bip_buffer_{};
        std::mutex mtx_{};
    };

    server_wire_container_impl(std::string_view name, std::string_view mutex_file, std::size_t datachannel_buffer_size, std::size_t max_datachannel_buffers, std::function<void(void)> clean_up)
        : name_(name), garbage_collector_impl_(std::make_unique<garbage_collector_impl>()), datachannel_buffer_size_(datachannel_buffer_size), clean_up_(std::move(clean_up)) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            boost::interprocess::permissions unrestricted_permissions;
            unrestricted_permissions.set_unrestricted();

            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), proportional_memory_size(datachannel_buffer_size_, max_datachannel_buffers), nullptr, unrestricted_permissions);
            auto req_wire = managed_shared_memory_->construct<tateyama::common::wire::unidirectional_message_wire>(tateyama::common::wire::request_wire_name)(managed_shared_memory_.get(), request_buffer_size);
            auto res_wire = managed_shared_memory_->construct<tateyama::common::wire::unidirectional_response_wire>(tateyama::common::wire::response_wire_name)(managed_shared_memory_.get(), response_buffer_size);
            status_provider_ = managed_shared_memory_->construct<tateyama::common::wire::status_provider>(tateyama::common::wire::status_provider_name)(managed_shared_memory_.get(), mutex_file);

            request_wire_.initialize(req_wire, req_wire->get_bip_address(managed_shared_memory_.get()));
            response_wire_.initialize(res_wire, res_wire->get_bip_address(managed_shared_memory_.get()));
        } catch(const boost::interprocess::interprocess_exception& ex) {
            std::stringstream ss{};
            ss << "failed to allocate shared memory in IPC endpoint: " << ex.what();
            throw std::runtime_error(ss.str());
        } catch (const std::exception &ex) {
            std::stringstream ss{};
            ss << "failed to allocate shared memory in IPC endpoint: " << ex.what();
            throw std::runtime_error(ss.str());
        }
    }
    server_wire_container_impl(std::string_view name, std::string_view mutex_file, std::size_t datachannel_buffer_size, std::size_t max_datachannel_buffers) : server_wire_container_impl(name, mutex_file, datachannel_buffer_size, max_datachannel_buffers, [](){}) {}  // for tests

    /**
     * @brief Copy and move constructers are deleted.
     */
    server_wire_container_impl(server_wire_container_impl const&) = delete;
    server_wire_container_impl(server_wire_container_impl&&) = delete;
    server_wire_container_impl& operator = (server_wire_container_impl const&) = delete;
    server_wire_container_impl& operator = (server_wire_container_impl&&) = delete;

    ~server_wire_container_impl() override {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        clean_up_();
    }

    wire_container* get_request_wire() override { return &request_wire_; }
    response_wire_container& get_response_wire() override { return response_wire_; }

    unq_p_resultset_wires_conteiner create_resultset_wires(std::string_view name, std::size_t count) override {
        try {
            return std::unique_ptr<resultset_wires_container_impl, resultset_deleter_type>{
                new resultset_wires_container_impl{managed_shared_memory_.get(), name, count, mtx_shm_, datachannel_buffer_size_}, resultset_deleter_impl};
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            LOG_LP(ERROR) << "running out of boost managed shared memory";
            throw std::runtime_error(ex.what());
        }
    }
    garbage_collector* get_garbage_collector() override {
        return garbage_collector_impl_.get();
    }

    static std::size_t proportional_memory_size(std::size_t datachannel_buffer_size, std::size_t max_datachannel_buffers) {
        return (datachannel_buffer_size + data_channel_overhead) * max_datachannel_buffers + (request_buffer_size + response_buffer_size) + total_overhead;
    }

    void expiration_time_over() override {
        garbage_collector_impl_->expiration_time_over();
    }

    // for client
    std::unique_ptr<resultset_wires_container_impl> create_resultset_wires_for_client(std::string_view name) {
        return std::make_unique<resultset_wires_container_impl>(managed_shared_memory_.get(), name, mtx_shm_);
    }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    wire_container_impl request_wire_{};
    response_wire_container_impl response_wire_{};
    tateyama::common::wire::status_provider* status_provider_{};
    std::unique_ptr<garbage_collector_impl> garbage_collector_impl_;
    std::mutex mtx_shm_{};

    std::size_t datachannel_buffer_size_;
    const std::function<void(void)> clean_up_;
};

inline void server_wire_container_impl::resultset_wire_container_impl::release(unq_p_resultset_wire_conteiner resultset_wire_conteiner) {
    resultset_wires_container_impl_.add_released_writer(std::move(resultset_wire_conteiner));
    if (thread_invoked_) {
        std::unique_lock<std::mutex> lock(mtx_queue_);

        released_ = true;
        cnd_queue_.notify_one();
        if (!queue_.empty()) {
            auto* current_annex = queue_.back().get();
            current_annex->notify();
        }
    } else {
        resultset_wires_container_impl_.write_complete();
    }
}
inline void server_wire_container_impl::resultset_wire_container_impl::write_complete() {
    resultset_wires_container_impl_.write_complete();
}

class connection_container
{
public:
    explicit connection_container(std::string_view name, std::size_t threads, std::size_t admin_sessions) : name_(name) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            boost::interprocess::permissions  unrestricted_permissions;
            unrestricted_permissions.set_unrestricted();

            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), fixed_memory_size(threads + admin_sessions), nullptr, unrestricted_permissions);
            managed_shared_memory_->destroy<tateyama::common::wire::connection_queue>(tateyama::common::wire::connection_queue::name);
            connection_queue_ = managed_shared_memory_->construct<tateyama::common::wire::connection_queue>(tateyama::common::wire::connection_queue::name)(threads, managed_shared_memory_->get_segment_manager(), static_cast<std::uint8_t>(admin_sessions));
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            using namespace std::literals::string_view_literals;

            std::stringstream ss{};
            ss << "cannot create a database connection outlet named "sv << name << ", probably another server is running using the same database name"sv;
            throw std::runtime_error(ss.str());
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
    tateyama::common::wire::connection_queue& get_connection_queue() {
        return *connection_queue_;
    }

    // for diagnostic
    [[nodiscard]] std::size_t pending_requests() const {
        return connection_queue_->pending_requests();
    }
    [[nodiscard]] std::size_t session_id_accepted() const {
        return connection_queue_->session_id_accepted();
    }

    static std::size_t fixed_memory_size(std::size_t n) {
        std::size_t size = initial_size + (n * per_size); // exact size
        size += initial_size / 2;                         // a little bit of leeway
        return ((size / 4096) + 1) * 4096;                // round up to the page size
    }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    tateyama::common::wire::connection_queue* connection_queue_;

    static constexpr std::size_t initial_size = 720;      // obtained by experiment
    static constexpr std::size_t per_size = 112;          // obtained by experiment
};

};  // namespace tateyama::common::wire
