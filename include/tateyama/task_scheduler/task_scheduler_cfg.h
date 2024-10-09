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

#include <cstddef>
#include <iomanip>

#include <boost/rational.hpp>

namespace tateyama::task_scheduler {

/**
 * @brief task scheduler configuration
 * @details getters specified with const are thread safe
 */
class task_scheduler_cfg {
public:
    using rational = boost::rational<std::int64_t>;

    static constexpr std::size_t numa_node_unspecified = static_cast<std::size_t>(-1);

    [[nodiscard]] std::size_t thread_count() const noexcept {
        return thread_count_;
    }

    void thread_count(std::size_t arg) noexcept {
        thread_count_ = arg;
    }

    [[nodiscard]] bool core_affinity() const noexcept {
        return set_core_affinity_;
    }

    void core_affinity(bool arg) noexcept {
        set_core_affinity_ = arg;
    }

    [[nodiscard]] std::size_t initial_core() const noexcept {
        return initial_core_;
    }

    void initial_core(std::size_t arg) noexcept {
        initial_core_ = arg;
    }

    [[nodiscard]] bool assign_numa_nodes_uniformly() const noexcept {
        return assign_numa_nodes_uniformly_;
    }

    void assign_numa_nodes_uniformly(bool arg) noexcept {
        assign_numa_nodes_uniformly_ = arg;
    }

    [[nodiscard]] std::size_t force_numa_node() const noexcept {
        return force_numa_node_;
    }

    void force_numa_node(std::size_t arg) noexcept {
        force_numa_node_ = arg;
    }

    [[nodiscard]] bool stealing_enabled() const noexcept {
        return stealing_enabled_;
    }

    void stealing_enabled(bool arg) noexcept {
        stealing_enabled_ = arg;
    }

    /**
     * @brief accessor for use_preferred_worker_for_current_thread flag
     * @return whether use_preferred_worker_for_current_thread is enabled for max performance from same client thread
     * @note this is experimental feature and will be dropped soon
     */
    [[nodiscard]] bool use_preferred_worker_for_current_thread() const noexcept {
        return use_preferred_worker_for_current_thread_;
    }

    /**
     * @brief setter for use_preferred_worker_for_current_thread flag
     * @note this is experimental feature and will be dropped soon
     */
    void use_preferred_worker_for_current_thread(bool arg) noexcept {
        use_preferred_worker_for_current_thread_ = arg;
    }

    /**
     * @brief accessor for ratio_check_local_first configuration
     * @return the ratio how frequently local task queue should be checked first.
     * @details scheduler checks both sticky queue and local queue on process_next is called. For fairness, the order
     * is changed whether sticky queue comes first or local queue does.
     * The rational number N/M for this configuration indicates N times out of M process_next calls check local task first.
     * This number must be in range [0, 1). If it's set to 0, only sticky queue is checked (no fairness).
     * Normally this number is expected to be equal or less than 1/2 because sticky queue has higher priority than local.
     */
    [[nodiscard]] rational ratio_check_local_first() const noexcept {
        return ratio_check_local_first_;
    }

    /**
     * @brief setter for ratio_check_local_first
     */
    void ratio_check_local_first(rational arg) noexcept {
        BOOST_ASSERT(ratio_check_local_first_ >= 0);  //NOLINT
        BOOST_ASSERT(ratio_check_local_first_ < 1);  //NOLINT

        ratio_check_local_first_ = arg;
    }

    [[nodiscard]] std::size_t stealing_wait() const noexcept {
        return stealing_wait_;
    }

    void stealing_wait(std::size_t arg) noexcept {
        stealing_wait_ = arg;
    }

    [[nodiscard]] std::size_t task_polling_wait() const noexcept {
        return task_polling_wait_;
    }

    void task_polling_wait(std::size_t arg) noexcept {
        task_polling_wait_ = arg;
    }

    /**
     * @brief accessor for busy worker flag
     * @return whether busy worker is enabled to frequently check task queues
     * @note this is experimental feature and will be dropped soon
     */
    [[nodiscard]] bool busy_worker() const noexcept {
        return busy_worker_;
    }

    /**
     * @brief setter for busy worker flag
     * @note this is experimental feature and will be dropped soon
     */
    void busy_worker(bool arg) noexcept {
        busy_worker_ = arg;
    }

    [[nodiscard]] std::size_t watcher_interval() const noexcept {
        return watcher_interval_;
    }

    void watcher_interval(std::size_t arg) noexcept {
        watcher_interval_ = arg;
    }


    [[nodiscard]] std::size_t worker_try_count() const noexcept {
        return worker_try_count_;
    }

    void worker_try_count(std::size_t arg) noexcept {
        worker_try_count_ = arg;
    }


    [[nodiscard]] std::size_t worker_suspend_timeout() const noexcept {
        return worker_suspend_timeout_;
    }

    void worker_suspend_timeout(std::size_t arg) noexcept {
        worker_suspend_timeout_ = arg;
    }

    friend inline std::ostream& operator<<(std::ostream& out, task_scheduler_cfg const& cfg) {
        return out << std::boolalpha <<
            "thread_count:" << cfg.thread_count() << " " <<
            "set_core_affinity:" << cfg.core_affinity() << " " <<
            "initial_core:" << cfg.initial_core() << " " <<
            "assign_numa_nodes_uniformly:" << cfg.assign_numa_nodes_uniformly() << " " <<
            "force_numa_node:" << (cfg.force_numa_node() == numa_node_unspecified ? "unspecified" : std::to_string(cfg.force_numa_node())) << " " <<
            "stealing_enabled:" << cfg.stealing_enabled() << " " <<
            "use_preferred_worker_for_current_thread:" << cfg.use_preferred_worker_for_current_thread() << " " <<
            "ratio_check_local_first:" << cfg.ratio_check_local_first() << " " <<
            "stealing_wait:" << cfg.stealing_wait() << " " <<
            "task_polling_wait:" << cfg.task_polling_wait() << " " <<
            "busy_worker:" << cfg.busy_worker() << " " <<
            "watcher_interval:" << cfg.watcher_interval() << " " <<
            "worker_try_count:" << cfg.worker_try_count() << " " <<
            "worker_suspend_timeout:" << cfg.worker_suspend_timeout() << " " <<
            "";
    }

private:
    std::size_t thread_count_ = 5;
    bool set_core_affinity_ = true;
    std::size_t initial_core_ = 1;
    bool assign_numa_nodes_uniformly_ = true;
    std::size_t force_numa_node_ = numa_node_unspecified;
    bool stealing_enabled_ = true;
    bool use_preferred_worker_for_current_thread_ = false;
    rational ratio_check_local_first_{1, 10};
    std::size_t stealing_wait_ = 1;
    std::size_t task_polling_wait_ = 0;
    bool busy_worker_ = false;
    std::size_t watcher_interval_ = 1000;
    std::size_t worker_try_count_ = 1000;
    std::size_t worker_suspend_timeout_ = 1000000;
};

}

