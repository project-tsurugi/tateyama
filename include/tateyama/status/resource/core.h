/*
 * Copyright 2018-2022 tsurugi project.
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

#include <string_view>
#include <sys/types.h>
#include <unistd.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/memory_order.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include <tateyama/framework/component.h>

namespace tateyama::status_info {

/**
 * @brief status code which describe status of resources
 */
enum class state : std::int64_t {
    initial = 0,

    // normal
    ready = 1,
    activated = 2,
    deactivating = 3,
    deactivated = 4,
};

class resource_status_memory {
  public:
    static constexpr std::string_view area_name = "status_info";

    // placed on a boost shared memory
    class resource_status {
      public:
        using void_allocator = boost::interprocess::allocator<void, boost::interprocess::managed_shared_memory::segment_manager>;

        using key_type = tateyama::framework::component::id_type;
        using value_type = state;
        using map_value_type = std::pair<const key_type, value_type>;
        using shmem_allocator = boost::interprocess::allocator<map_value_type, boost::interprocess::managed_shared_memory::segment_manager>;
        using shared_memory_map = boost::interprocess::map<key_type, value_type, std::less<>, shmem_allocator>;
    
        explicit resource_status(const void_allocator& allocator)
            : resource_status_map_(allocator), service_status_map_(allocator), endpoint_status_map_(allocator) {
            shutdown_requested_.store(false, boost::memory_order_relaxed);
        }
    
      private:
        [[nodiscard]] bool test_and_set_shutdown() {
            bool expected = false;
            return shutdown_requested_.compare_exchange_strong(expected, true, boost::memory_order_acq_rel);
        }
        [[nodiscard]] bool request_shutdown() {
            boost::interprocess::scoped_lock lock(m_shutdown_);
            if (!test_and_set_shutdown()) {
                return false;
            }
            lock.unlock();
            c_shutdown_.notify_one();
            return true;
        }
        void wait_for_shutdown() {
            boost::interprocess::scoped_lock lock(m_shutdown_);
            while (!shutdown_requested_.load(boost::memory_order_acquire)) {
                c_shutdown_.wait(lock);
            }
            lock.unlock();
        }

        shared_memory_map resource_status_map_;
        shared_memory_map service_status_map_;
        shared_memory_map endpoint_status_map_;
        state whole_{};
        pid_t pid_{};
        boost::atomic<bool> shutdown_requested_{};
        boost::interprocess::interprocess_mutex m_shutdown_{};
        boost::interprocess::interprocess_condition c_shutdown_{};

        friend class resource_status_memory;
    };

    // serves as a relay for access to resource_status
    explicit resource_status_memory(boost::interprocess::managed_shared_memory& mem, bool owner = true) : mem_(mem), owner_(owner) {
        std::string name(area_name);
        if (owner) {
            mem_.destroy<resource_status>(name.c_str());
            resource_status_ = mem_.construct<resource_status>(name.c_str())(mem_.get_segment_manager());
        } else {
            resource_status_ = mem_.find<resource_status>(name.c_str()).first;
        }
    }
    ~resource_status_memory() {
        if (owner_ && resource_status_) {
            mem_.destroy<resource_status>(std::string(area_name).c_str());
        }
    }

    resource_status_memory(resource_status_memory const& other) = delete;
    resource_status_memory& operator=(resource_status_memory const& other) = delete;
    resource_status_memory(resource_status_memory&& other) noexcept = delete;
    resource_status_memory& operator=(resource_status_memory&& other) noexcept = delete;
    
    void set_pid() {
        resource_status_->pid_ = ::getpid();
    }
    [[nodiscard]] pid_t pid() const {
        return resource_status_->pid_;
    }
    void whole(state s) {
        resource_status_->whole_ = s;
    }
    [[nodiscard]] state whole() const {
        return resource_status_->whole_;
    }
    [[nodiscard]] bool valid() {
        return resource_status_ != nullptr;
    }
    [[nodiscard]] bool is_shutdown_requested() {
        return resource_status_->shutdown_requested_.load(boost::memory_order_acquire);
    }
    [[nodiscard]] bool request_shutdown() {
        return resource_status_->request_shutdown();
    }
    void wait_for_shutdown() {
        resource_status_->wait_for_shutdown();
    }
    // obsolete
    [[nodiscard]] bool shutdown() {
        return is_shutdown_requested();
            }

private:
    resource_status* resource_status_{};
    boost::interprocess::managed_shared_memory& mem_;
    bool owner_{};
};

} // namespace tateyama::status_info
