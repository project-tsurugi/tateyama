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

#include <sched.h>
#include <pthread.h>
#include <numa.h>

namespace tateyama::utils {

enum class affinity_kind {
    undefined,
    core_affinity,
    numa_affinity
};

template<affinity_kind Kind>
struct affinity_tag_t {
    constexpr explicit affinity_tag_t() = default;
};

template<affinity_kind Kind>
constexpr inline affinity_tag_t<Kind> affinity_tag{};

/**
 * @brief profile to define the cpu affinity
 */
class affinity_profile {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    /**
     * @brief default profile - no core affinity defined
     */
    affinity_profile() = default;

    /**
     * @brief profile to assign numa node
     * @param numa_node specify the numa node number (0-origin). If not present, assign to numa nodes uniformly.
     */
    explicit affinity_profile(
        affinity_tag_t<affinity_kind::numa_affinity>,
        std::size_t numa_node = npos
    );

    /**
     * @brief profile to assign cores sequentially from initial one
     * @param initial_core the core number (0-origin) that indicates the beginning. Cores are assigned
     * by incrementing the number one by one.
     */
    affinity_profile(
        affinity_tag_t<affinity_kind::core_affinity>,
        std::size_t initial_core
    );

    friend bool set_thread_affinity(std::size_t id, affinity_profile const& prof);

private:
    bool set_core_affinity_{false};
    bool assign_numa_nodes_uniformly_{false};
    std::size_t numa_node_{npos};
    std::size_t initial_core_{npos};

    affinity_profile(
        bool set_core_affinity,
        bool assign_numa_nodes_uniformly,
        std::size_t numa_node,
        std::size_t initial_core
    );

};

/**
 * @brief set cpu affinity for the current thread
 * @param id the 0-origin integer associated with current thread indicating that the
 * current thread is `id`-th within the set of threads that are going to be assigned
 * @param prof the affinity profile on how the affinity are determined
 * @return true when successful
 * @return false otherwise
 */
bool set_thread_affinity(std::size_t id, affinity_profile const& prof);

}

