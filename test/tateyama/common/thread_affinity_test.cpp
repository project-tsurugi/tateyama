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

#include <regex>
#include <future>
#include <xmmintrin.h>
#include <gtest/gtest.h>
#include <gflags/gflags.h>

namespace tateyama::common {

using namespace std::literals::string_literals;
using clock = std::chrono::system_clock;

class thread_affinity_test : public ::testing::Test {
public:
    std::string getenv(std::string_view name, std::string_view default_value) {
        std::string n{name};
        auto v = std::getenv(n.data());
        if(v == nullptr) return std::string{default_value};
        return {v};
    }
    void SetUp() override {
        duration_ms_ = std::stoul(getenv("duration", "10"));
    }

    void execute(std::size_t tasks, utils::affinity_profile const& prof) {
        std::vector<std::future<void>> results{};
        results.reserve(tasks);
        for(std::size_t i=0; i < tasks; ++i) {
            results.emplace_back(std::async(std::launch::async, [=]() {
                if(! tateyama::utils::set_thread_affinity(i, prof)) {
                    FAIL();
                };
                auto b = clock::now();
                while(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -b).count() < duration_ms_) {
                    _mm_pause(); // spin so that the cpu activity is visible
                }
            }));
        }
        for(auto&& f : results) {
            f.get();
        }
    }
    std::size_t duration_ms_{};
};

using namespace std::string_view_literals;
using namespace tateyama::utils;

TEST_F(thread_affinity_test, default) {
    affinity_profile prof{};
    execute(1, prof);
}

TEST_F(thread_affinity_test, uniform) {
    if(numa_available() < 0) {
        GTEST_SKIP_("numa not available on this system");
    }
    affinity_profile prof{affinity_tag<affinity_kind::numa_affinity>}; // assign nodes uniformly
    execute(1, prof);
}

TEST_F(thread_affinity_test, uniform_2) {
    if(numa_available() < 0) {
        GTEST_SKIP_("numa not available on this system");
    }
    affinity_profile prof{affinity_tag<affinity_kind::numa_affinity>}; // assign nodes uniformly
    execute(2, prof);
}

TEST_F(thread_affinity_test, numa_0) {
    if(numa_available() < 0) {
        GTEST_SKIP_("numa not available on this system");
    }
    affinity_profile prof{affinity_tag<affinity_kind::numa_affinity>, 0}; // assign node 0
    execute(1, prof);
}

TEST_F(thread_affinity_test, numa_1) {
    if(numa_available() < 0) {
        GTEST_SKIP_("numa not available on this system");
    }
    // assign node 1 - fallback to 0 if numa node 1 is missing
    affinity_profile prof{affinity_tag<affinity_kind::numa_affinity>, 1};
    execute(1, prof);
}

TEST_F(thread_affinity_test, core_0) {
    affinity_profile prof{affinity_tag<affinity_kind::core_affinity>, 0}; // assign core 0
    execute(1, prof);
}

TEST_F(thread_affinity_test, core_1) {
    affinity_profile prof{affinity_tag<affinity_kind::core_affinity>, 1}; // assign core 0
    execute(1, prof);
}

TEST_F(thread_affinity_test, core_1_2) {
    affinity_profile prof{affinity_tag<affinity_kind::core_affinity>, 1}; // assign two from core 0
    execute(2, prof);
}

}
