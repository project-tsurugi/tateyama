/*
 * Copyright 2018-2021 tsurugi project.
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
#include <iostream>
#include <vector>
#include <chrono>
#include <future>
#include <xmmintrin.h>

#include <glog/logging.h>

#include <tateyama/utils/thread_affinity.h>

DEFINE_int64(duration, 5000, "Run duration in milli-seconds");  //NOLINT
DEFINE_int32(thread_count, 1, "Number of threads");  //NOLINT
DEFINE_bool(core_affinity, false, "Whether threads are assigned to cores");  //NOLINT
DEFINE_int32(initial_core, 0, "initial core number, that the bunch of cores assignment begins with");  //NOLINT
DEFINE_bool(minimum, false, "run with minimum amount of data");  //NOLINT
DEFINE_bool(assign_numa_nodes_uniformly, true, "assign cores uniformly on all numa nodes");  //NOLINT
DEFINE_int32(numa_node, -1, "numa node number. Specify -1 if no numa node is assigned");  //NOLINT

namespace tateyama::thread_affinity_cli {

using namespace std::string_literals;
using namespace std::string_view_literals;

using clock = std::chrono::system_clock;

utils::affinity_profile setup_affinity(
    bool set_core_affinity,
    bool assign_numa_nodes_uniformly,
    std::size_t numa_node,
    std::size_t initial_core
) {
    utils::affinity_profile prof{};
    if(numa_node != utils::affinity_profile::npos) {
        prof = utils::affinity_profile{
            utils::affinity_tag<utils::affinity_kind::numa_affinity>,
            numa_node
        };
    } else if (assign_numa_nodes_uniformly) {
        prof = utils::affinity_profile{
            utils::affinity_tag<utils::affinity_kind::numa_affinity>
        };
    } else if(set_core_affinity) {
        prof = utils::affinity_profile{
            utils::affinity_tag<utils::affinity_kind::core_affinity>,
            initial_core
        };
    }
    return prof;
}

class cli {
public:
    void run() {
        std::vector<std::future<void>> results{};
        auto prof = setup_affinity(
            FLAGS_core_affinity,
            FLAGS_assign_numa_nodes_uniformly,
            static_cast<std::size_t>(FLAGS_numa_node),
            static_cast<std::size_t>(FLAGS_initial_core)
        );
        auto duration = FLAGS_duration;
        if (FLAGS_minimum) {
            duration = 1;
        }
        for(std::size_t i=0, n=FLAGS_thread_count; i<n; ++i) {
            results.emplace_back(std::async(std::launch::async, [=](){
                if(! tateyama::utils::set_thread_affinity(i, prof)) {
                    LOG(ERROR) << "failed to set thread affinity";
                    return;
                };
                auto b = clock::now();
                while(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -b).count() < duration) {
                    _mm_pause(); // spin so that the cpu activity is visible
                }
            }));
        }
        for(auto&& f : results) {
            f.get();
        }
    }
};

}

extern "C" int main(int argc, char* argv[]) {
    // ignore log level
    if (FLAGS_log_dir.empty()) {
        FLAGS_logtostderr = true;
    }
    google::InitGoogleLogging("thread_affinity_cli");
    google::InstallFailureSignalHandler();
    gflags::SetUsageMessage("thread_affinity_cli");
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (argc != 1) {
        gflags::ShowUsageWithFlags(argv[0]); // NOLINT
        return -1;
    }
    try {
        tateyama::thread_affinity_cli::cli{}.run();  // NOLINT
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}
