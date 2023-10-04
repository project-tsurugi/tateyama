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
#include "tateyama/endpoint/ipc/server_client_base.h"

#include "bench_result.h"

static inline void ASSERT_OK(tateyama::status status) {
    if (status != tateyama::status::ok) {
        std::cout << boost::stacktrace::stacktrace();
        std::exit(1);
    }
}

namespace tateyama::api::endpoint::ipc {

class server_client_bench_base: public server_client_base {
public:
    server_client_bench_base(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc, int nthread) :
            server_client_base(cfg, nproc, nthread) {
    }

    void server() override {
        result_.start();
        server_client_base::server();
        result_.end();
    }

    void add_result_value(info_type type, double value) {
        result_.add(type, value);
    }

    double real_sec() {
        return result_.real_sec();
    }

    bench_result& result() {
        return result_;
    }

private:
    bench_result result_ { };
};

} // namespace tateyama::api::endpoint::ipc
