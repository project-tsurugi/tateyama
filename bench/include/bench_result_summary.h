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
#pragma once

#include <map>

#include "bench_result.h"

class bench_result_summary {
    enum class show_part {
        all, server, client
    };

public:
    bench_result_summary(std::vector<bool> &use_multi_thread_list, std::vector<int> &nsession_list,
            std::vector<std::size_t> &msg_len_list, bool with_respose = true) :
            use_multi_thread_list_(use_multi_thread_list), nsession_list_(nsession_list), msg_len_list_(msg_len_list), with_respose_(
                    with_respose) {
    }

    void add(bool use_multi_thread, int nsession, std::size_t msg_len, bench_result &result) {
        std::string k = key(use_multi_thread, nsession, msg_len);
        result_map_[k] = result.get_results();
    }

    void show(show_part part, info_type type, std::string_view data_unit, int prec) {
        for (bool use_multi_thread : use_multi_thread_list_) {
            std::cout << std::endl;
            std::cout << "# summary";
            std::cout << " : " << (use_multi_thread ? "multi_thread" : "multi_process");
            std::cout << " : " << (with_respose_ ? "with_response" : "without_response");
            switch (part) {
            case show_part::server:
                std::cout << " : server";
                break;
            case show_part::client:
                std::cout << " : client";
                break;
            }
            std::cout << " : " << data_unit << std::endl;
            std::cout << (use_multi_thread ? "MT" : "MP") << (with_respose_ ? "r" : "nr");
            for (std::size_t msg_len : msg_len_list_) {
                std::cout << ", " << msg_len;
            }
            std::cout << std::endl;
            //
            for (int nsession : nsession_list_) {
                std::cout << nsession;
                for (std::size_t msg_len : msg_len_list_) {
                    std::map<info_type, double> &map = result_map_[key(use_multi_thread, nsession, msg_len)];
                    std::cout << ",";
                    std::cout << std::fixed << std::setprecision(prec) << map[type];
                }
                std::cout << std::endl;
            }
        }
    }

    void show(info_type type, std::string_view data_unit, int prec) {
        show(show_part::all, type, data_unit, prec);
    }

    void show(sc_info_type sc_type, std::string_view data_unit, int prec) {
        show(show_part::server, get_info_type(true, sc_type), data_unit, prec);
        show(show_part::client, get_info_type(false, sc_type), data_unit, prec);
    }

    void show() {
        show(info_type::nloop, "# of loop", 0);
        show(info_type::real_sec, "real_time [sec]", 6);
        show(info_type::throughput_msg_per_sec, "throughput [msg/sec]", 0);
        show(info_type::throughput_gb_per_sec, "throughput [GB/sec]", 2);
        //
        show(sc_info_type::user_sec, "user_time [sec]", 6);
        show(sc_info_type::sys_sec, "system_time [sec]", 6);
        show(sc_info_type::nvcsw, "# of voluntary ctx.swc.", 0);
        show(sc_info_type::nivcsw, "# of involuntary ctx.swc.", 0);
    }
private:
    std::vector<bool> &use_multi_thread_list_;
    std::vector<int> &nsession_list_;
    std::vector<std::size_t> &msg_len_list_;
    bool with_respose_;
    std::map<std::string, std::map<info_type, double>> result_map_ { };

    inline std::string key(bool use_multi_thread, int nsession, std::size_t msg_len) {
        return std::to_string(nsession) + (use_multi_thread > 0 ? "mt" : "mp") + std::to_string(msg_len);
    }
};
