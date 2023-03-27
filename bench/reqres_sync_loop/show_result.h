/*
 * Copyright 2018-2023 tsurugi project.
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
#pragma

#include <vector>
#include <map>

inline std::string key(bool use_multi_thread, int nsession, std::size_t msg_len) {
    return std::to_string(nsession) + (use_multi_thread > 0 ? "mt" : "mp") + std::to_string(msg_len);
}

inline void show_result_summary(std::vector<bool> use_multi_thread_list, std::vector<int> nsession_list,
        std::vector<std::size_t> msg_len_list, std::map<std::string, double> results, std::string_view unit, int prec =
                1) {
    for (bool use_multi_thread : use_multi_thread_list) {
        std::cout << std::endl;
        std::cout << "# summary : " << (use_multi_thread > 0 ? "multi_thread" : "multi_process");
        std::cout << " " << unit << std::endl;
        std::cout << "# nsession \\ msg_len";
        for (std::size_t msg_len : msg_len_list) {
            std::cout << ", " << msg_len;
        }
        std::cout << std::endl;
        //
        for (int nsession : nsession_list) {
            std::cout << nsession;
            for (std::size_t msg_len : msg_len_list) {
                double r = results[key(use_multi_thread, nsession, msg_len)];
                std::cout << "," << std::fixed << std::setprecision(prec) << r;
            }
            std::cout << std::endl;
        }
    }
}
