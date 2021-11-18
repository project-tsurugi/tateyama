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
#include <tateyama/utils/thread_affinity.h>

#include <sched.h>
#include <pthread.h>
#include <numa.h>

namespace tateyama::utils {

static std::size_t numa_node_count() {
    static std::size_t nodes = numa_max_node()+1;
    return nodes;
}

bool set_thread_affinity(std::size_t id, affinity_profile const& prof) {
    if(! prof.set_core_affinity_) return true;
    auto nodes = numa_node_count();
    if(prof.assign_numa_nodes_uniformly_) {
        return 0 == numa_run_on_node(static_cast<int>(id % nodes));
    }
    if(prof.numa_node_ != affinity_profile::npos) {
        // round down if specified value is larger than nodes
        return 0 == numa_run_on_node(static_cast<int>(prof.numa_node_ % nodes));
    }
    cpu_set_t cpuset{};
    CPU_ZERO(&cpuset);
    CPU_SET(id + prof.initial_core_, &cpuset);  //NOLINT
    return 0 == ::pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

affinity_profile::affinity_profile(
    bool set_core_affinity,
    bool assign_numa_nodes_uniformly,
    std::size_t numa_node,
    std::size_t initial_core
) :
    set_core_affinity_(set_core_affinity),
    assign_numa_nodes_uniformly_(assign_numa_nodes_uniformly),
    numa_node_(numa_node),
    initial_core_(initial_core)
{}

affinity_profile::affinity_profile(
    affinity_tag_t<affinity_kind::numa_affinity>,
    std::size_t numa_node
) :
    affinity_profile(
        true,
        numa_node == npos,
        numa_node,
        npos
    )
{}

affinity_profile::affinity_profile(
    affinity_tag_t<affinity_kind::core_affinity>,
    std::size_t initial_core
) :
    affinity_profile(
        true,
        false,
        npos,
        initial_core
    )
{}

}

