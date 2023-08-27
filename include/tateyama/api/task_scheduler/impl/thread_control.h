/*
 * Copyright 2018-2020 tsurugi project.
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

#include <cstdint>
#include <functional>
#include <condition_variable>
#include <atomic>

#include <boost/thread.hpp>
#include <glog/logging.h>
#include <numa.h>

#include <tateyama/api/task_scheduler/task_scheduler_cfg.h>
#include <tateyama/api/task_scheduler/impl/thread_initialization_info.h>
#include <tateyama/utils/thread_affinity.h>
#include <tateyama/utils/cache_align.h>
#include <tateyama/utils/hex.h>
#include "utils.h"
#include <tateyama/common.h>

namespace tateyama::task_scheduler {

using tateyama::api::task_scheduler::task_scheduler_cfg;

// separating mutex and cv from thread in order to make thread movable
struct cache_align cv {
    std::condition_variable cv_{};
    std::mutex mutex_{};
};

/**
 * @brief physical thread control object
 */
class cache_align thread_control {
public:

    /**
     * @brief undefined thread id
     */
    constexpr static std::size_t undefined_thread_id = static_cast<std::size_t>(-1);

    /**
     * @brief construct empty instance
     */
    thread_control() = default;
    ~thread_control() = default;
    thread_control(thread_control const& other) = delete;
    thread_control& operator=(thread_control const& other) = delete;
    thread_control(thread_control&& other) noexcept = default;
    thread_control& operator=(thread_control&& other) noexcept = default;

    template <class F, class ...Args, class = std::enable_if_t<std::is_invocable_v<F, Args...>>>
    explicit thread_control(std::size_t thread_id, task_scheduler_cfg const* cfg, F&& callable, Args&&...args) :
        origin_(create_thread_body(thread_id, cfg, std::forward<F>(callable), std::forward<Args>(args)...))
    {}

    template <class F, class ...Args, class = std::enable_if_t<std::is_invocable_v<F, Args...>>>
    explicit thread_control(F&& callable, Args&&...args) :
        thread_control(undefined_thread_id , nullptr, std::forward<F>(callable), std::forward<Args>(args)...)
    {}

    void join() {
        origin_.join();
        assert(! active());  //NOLINT
    }

    [[nodiscard]] bool active() const noexcept {
        std::unique_lock lk{sleep_cv_->mutex_};
        return active_;
    }

    /**
     * @brief wait initialization of the thread
     * @details if init() is implemented by thread body callable, it will be called on the newly created thread
     * at the construction of this object. This function is to synchronously wait for the init() call.
     * This is useful especially when multiple thread_control objects share an object and initialization needs to be
     * done before any of the thread_control is activated.
     */
    void wait_initialization() noexcept {
        std::unique_lock lk{initialized_cv_->mutex_};
        initialized_cv_->cv_.wait(lk, [this]() {
            return initialized_;
        });
    }

    void activate() noexcept {
        {
            std::unique_lock lk{sleep_cv_->mutex_};
            if(*completed_) return;
            if(active_) return;
            active_ = true;
        }
        sleep_cv_->cv_.notify_all();
    }

    template<typename Rep = std::chrono::hours::rep, typename Period = std::chrono::hours::period>
    void suspend(std::chrono::duration<Rep, Period> timeout = std::chrono::hours{24}) {
        std::unique_lock lk{sleep_cv_->mutex_};
        if(*completed_) return;
        active_ = false;
        sleep_cv_->cv_.wait_for(lk, timeout, [this]() {
            return active_;
        });
    }

    void print_diagnostic(std::ostream& os) {
        os << "      physical_id: " << utils::hex(origin_.get_id()) << std::endl;
    }

    bool completed() noexcept {
        std::unique_lock lk{sleep_cv_->mutex_};
        return (*completed_);
    }
private:
    std::unique_ptr<cv> sleep_cv_{std::make_unique<cv>()};
    bool active_{};
    std::unique_ptr<cv> initialized_cv_{std::make_unique<cv>()};
    bool initialized_{};
    std::unique_ptr<std::atomic_bool> completed_{std::make_unique<std::atomic_bool>()};

    // thread must come last since the construction starts new thread, which accesses the member variables above.
    boost::thread origin_{};

    bool setup_core_affinity(std::size_t id, api::task_scheduler::task_scheduler_cfg const* cfg) {
        if (! cfg) return false;
        utils::affinity_profile prof{};
        if(cfg->force_numa_node() != task_scheduler_cfg::numa_node_unspecified) {
            prof = utils::affinity_profile{
                utils::affinity_tag<utils::affinity_kind::numa_affinity>,
                cfg->force_numa_node()
            };
        } else if (cfg->assign_numa_nodes_uniformly()) {
            prof = utils::affinity_profile{
                utils::affinity_tag<utils::affinity_kind::numa_affinity>
            };
        } else if(cfg->core_affinity()) {
            prof = utils::affinity_profile{
                utils::affinity_tag<utils::affinity_kind::core_affinity>,
                cfg->initial_core()
            };
        }
        return utils::set_thread_affinity(id, prof);
    }

    template <class F, class ...Args, class = std::enable_if_t<std::is_invocable_v<F, Args...>>>
    auto create_thread_body(std::size_t thread_id, task_scheduler_cfg const* cfg, F&& callable, Args&&...args) {
        // libnuma initialize some static variables on the first call numa_node_of_cpu(). To avoid multiple threads race initialization, call it here.
        numa_node_of_cpu(sched_getcpu());

        // C++20 supports forwarding captured parameter packs. Use forward as tuple for now.
        return [=, args=std::tuple<Args...>(std::forward<Args>(args)...)]() mutable { // assuming args are copyable
            //// temporarily stop renaming the thread - Frame graph accidentally take the thread name as function name
            //std::string name("Id"+std::to_string(thread_id));
            //pthread_setname_np(origin_.native_handle(), name.c_str());
            setup_core_affinity(thread_id, cfg);
            if constexpr (has_init_v<F, Args...>) {
                thread_initialization_info info{thread_id, this};
                std::apply([&callable, &info](auto&& ...args) {
                    callable.init(info, args...);
                }, args);
            }
            {
                std::unique_lock lk{initialized_cv_->mutex_};
                initialized_ = true;
            }
            initialized_cv_->cv_.notify_all();
            {
                std::unique_lock lk{sleep_cv_->mutex_};
                sleep_cv_->cv_.wait(lk, [&] {
                    return active_;
                });
            }
            DLOG(INFO) << "thread " << thread_id
                << " physical_id:" << utils::hex(boost::this_thread::get_id())
                << " runs on cpu:" << sched_getcpu()
                << " node:" << numa_node_of_cpu(sched_getcpu());
            std::apply([&callable](auto&& ...args) {
                callable(args...);
            }, std::move(args));
            {
                std::unique_lock lk{sleep_cv_->mutex_};
                *completed_ = true;
                active_ = false;
            }
        };
    }
};

}
