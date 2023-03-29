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

#include <map>

#include <sys/time.h>
#include <sys/resource.h>

#include "show_result.h"

class rusage_diff {
public:
    rusage_diff(int who) :
            who_(who) {
        get_rusage(&rusage_start_);
    }

    struct rusage& diff() {
        get_rusage(&rusage_end_);
        minus(rusage_start_.ru_utime, rusage_end_.ru_utime, rusage_diff_.ru_utime);
        minus(rusage_start_.ru_stime, rusage_end_.ru_stime, rusage_diff_.ru_stime);
        rusage_diff_.ru_maxrss = rusage_end_.ru_maxrss - rusage_start_.ru_maxrss;
        rusage_diff_.ru_nvcsw = rusage_end_.ru_nvcsw - rusage_start_.ru_nvcsw;
        rusage_diff_.ru_nivcsw = rusage_end_.ru_nivcsw - rusage_start_.ru_nivcsw;
        return rusage_diff_;
    }

private:
    void get_rusage(struct rusage *usage) {
        getrusage(who_, usage);
    }

    static const long sec2micro = 1'000'000;

    long tv2long(struct timeval &tv) {
        return (sec2micro * tv.tv_sec + tv.tv_usec);
    }

    void long2tv(long t, struct timeval &tv) {
        tv.tv_sec = (t / sec2micro);
        tv.tv_usec = (t % sec2micro);
    }

    void minus(struct timeval &start, struct timeval &end, struct timeval &result) {
        long t1 = tv2long(start);
        long t2 = tv2long(end);
        long t3 = t2 - t1;
        long2tv(t3, result);
    }

    int who_ { };
    struct rusage rusage_start_ { };
    struct rusage rusage_end_ { };
    struct rusage rusage_diff_ { };
};

inline void output_timeval(struct timeval &tv, std::ostream &cout) {
    double t = tv.tv_sec + 1e-6 * tv.tv_usec;
    cout << std::fixed << std::setprecision(6) << t;
}

inline void output_utime(struct rusage &usage, std::ostream &cout) {
    output_timeval(usage.ru_utime, cout);
}

inline void output_stime(struct rusage &usage, std::ostream &cout) {
    output_timeval(usage.ru_stime, cout);
}

inline void output_maxrss(struct rusage &usage, std::ostream &cout) {
    cout << usage.ru_maxrss;
}

inline void output_nvcsw(struct rusage &usage, std::ostream &cout) {
    cout << usage.ru_nvcsw;
}

inline void output_nivcsw(struct rusage &usage, std::ostream &cout) {
    cout << usage.ru_nivcsw;
}

class resource_summary {
public:
    void add(std::string key, struct rusage &server, struct rusage &client) {
        rusage_servers_[key] = server;
        rusage_clients_[key] = client;
    }

    void output(std::vector<bool> use_multi_thread_list, std::vector<int> nsession_list,
            std::vector<std::size_t> msg_len_list, bool show_server,
            void (*output_func)(struct rusage &usage, std::ostream &cout), std::string_view data_unit) {
        std::map<std::string, struct rusage> &map = (show_server ? rusage_servers_ : rusage_clients_);
        for (bool use_multi_thread : use_multi_thread_list) {
            std::cout << std::endl;
            std::cout << "# resource summary : " << (show_server ? "server" : "clients");
            std::cout << " : " << (use_multi_thread > 0 ? "multi_thread" : "multi_process");
            std::cout << " : " << data_unit << std::endl;
            std::cout << "# nsession \\ msg_len";
            for (std::size_t msg_len : msg_len_list) {
                std::cout << ", " << msg_len;
            }
            std::cout << std::endl;
            //
            for (int nsession : nsession_list) {
                std::cout << nsession;
                for (std::size_t msg_len : msg_len_list) {
                    struct rusage &usage = map[key(use_multi_thread, nsession, msg_len)];
                    std::cout << ",";
                    output_func(usage, std::cout);
                }
                std::cout << std::endl;
            }
        }
    }

    void output(std::vector<bool> use_multi_thread_list, std::vector<int> nsession_list,
            std::vector<std::size_t> msg_len_list, void (*output_func)(struct rusage &usage, std::ostream &cout),
            std::string_view data_unit) {
        output(use_multi_thread_list, nsession_list, msg_len_list, true, output_func, data_unit);
        output(use_multi_thread_list, nsession_list, msg_len_list, false, output_func, data_unit);
    }

    void output(std::vector<bool> use_multi_thread_list, std::vector<int> nsession_list,
            std::vector<std::size_t> msg_len_list) {
        output(use_multi_thread_list, nsession_list, msg_len_list, output_utime, "user_time [sec]");
        output(use_multi_thread_list, nsession_list, msg_len_list, output_stime, "system_time [sec]");
        // output(use_multi_thread_list, nsession_list, msg_len_list, output_maxrss, "maxrss [byte]");
        output(use_multi_thread_list, nsession_list, msg_len_list, output_nvcsw, "# of voluntary ctx.swc.");
        output(use_multi_thread_list, nsession_list, msg_len_list, output_nivcsw, "# of involuntary ctx.swc.");
    }

private:
    std::map<std::string, struct rusage> rusage_servers_ { };
    std::map<std::string, struct rusage> rusage_clients_ { };
};
