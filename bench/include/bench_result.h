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

#include "tateyama/endpoint/ipc/ipc_test_utils.h"

#include "rusage_diff.h"

enum class info_type {
    nloop, real_sec, throughput_msg_per_sec, throughput_gb_per_sec,
    //
    server_sys_sec,
    server_user_sec,
    server_nvcsw,
    server_nivcsw,
    //
    client_sys_sec,
    client_user_sec,
    client_nvcsw,
    client_nivcsw,
    //
    program_error
};

enum class sc_info_type {
    sys_sec, user_sec, nvcsw, nivcsw
};

static inline info_type get_info_type(bool server, sc_info_type type) {
    switch (type) {
    case sc_info_type::sys_sec:
        return (server ? info_type::server_sys_sec : info_type::client_sys_sec);
    case sc_info_type::user_sec:
        return (server ? info_type::server_user_sec : info_type::client_user_sec);
    case sc_info_type::nvcsw:
        return (server ? info_type::server_nvcsw : info_type::client_nvcsw);
    case sc_info_type::nivcsw:
        return (server ? info_type::server_nivcsw : info_type::client_nivcsw);
    }
    return info_type::program_error;
}

class bench_result {
public:
    bench_result() :
            r_server_(true), r_clients_(false) {
    }

    void start() {
        r_server_.start();
        r_clients_.start();
        elapse_.start();
    }

    void end() {
        elapse_.stop();
        r_server_.end();
        r_clients_.end();
        //
        add(info_type::real_sec, elapse_.sec());
        add_rusage();
    }

    void add(info_type type, double value) {
        results_[type] = value;
    }

    std::map<info_type, double>& get_results() {
        return results_;
    }

    double real_sec() {
        return results_[info_type::real_sec];
    }

private:
    tateyama::api::endpoint::ipc::elapse elapse_;
    rusage_diff r_server_;
    rusage_diff r_clients_;
    std::map<info_type, double> results_ { };

    double tv2sec(struct timeval &tv) {
        return (tv.tv_sec + 1e-6 * tv.tv_usec);
    }

    void add_rusage() {
        {
            struct rusage &server = r_server_.get_diff();
            add(info_type::server_sys_sec, tv2sec(server.ru_stime));
            add(info_type::server_user_sec, tv2sec(server.ru_utime));
            add(info_type::server_nvcsw, server.ru_nvcsw);
            add(info_type::server_nivcsw, server.ru_nivcsw);
        }
        {
            struct rusage &clients = r_clients_.get_diff();
            add(info_type::client_sys_sec, tv2sec(clients.ru_stime));
            add(info_type::client_user_sec, tv2sec(clients.ru_utime));
            add(info_type::client_nvcsw, clients.ru_nvcsw);
            add(info_type::client_nivcsw, clients.ru_nivcsw);
        }
    }
};
