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
#include <tateyama/framework/server.h>
#include "ipc_test_utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace tateyama::endpoint::ipc {

class server_client_base {
public:
    server_client_base(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nproc = 1,
            int nthread = 0) :
            cfg_(cfg), nproc_(nproc), nthread_(nthread) {
        // nthread_ == 0 means not invoke a worker thread (i.e. using main thread)
        nworker_ = nproc_ * std::max(nthread_, 1);
    }

    virtual std::shared_ptr<tateyama::framework::service> create_server_service() = 0;
    virtual void start_server_client();
    virtual void server();
    virtual void server_dump(const std::size_t msg_num, const std::size_t len_sum);
    virtual void client();
    virtual void client_thread() = 0;

protected:
    std::shared_ptr<tateyama::api::configuration::whole> const &cfg_;
    int nproc_;
    int nthread_;
    int nworker_;
    elapse server_elapse_ { };

    virtual void check_client_exitcode(int code) {
        // nothing to do
        // NOTE: just exists for server_client_gtest_base::check_client_exitcode()
    }

    virtual void client_exit() {
        // NOTE: just exists for server_client_gtest_base::client_exit()
        std::exit(0);
    }

private:
    // for server process
    void wait_client_exit();
    void server_startup_start();
    void server_startup_end();
    // for client process
    void wait_server_startup_end();

    std::vector<pid_t> client_pids_ { };
    std::vector<std::thread> threads_ { };
    std::string lock_filename_ { };
    int fd_ { };
};

} // namespace tateyama::endpoint::ipc
