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
#include <tateyama/framework/server.h>
#include "ipc_test_utils.h"

namespace tateyama::api::endpoint::ipc {

class server_client_base {
public:
    server_client_base(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int nclient = 1) :
            cfg_(cfg), nclient_(nclient) {
    }

    virtual std::shared_ptr<tateyama::framework::service> create_server_service() = 0;
    virtual void start_server_client();
    virtual void server();
    virtual void server_dump(const std::size_t msg_num, const std::size_t len_sum);
    virtual void client() = 0;

protected:
    std::shared_ptr<tateyama::api::configuration::whole> const &cfg_;
    int nclient_ { };
    std::vector<pid_t> client_pids_ { };
    elapse server_elapse_;

    void wait_client_exit();
};

} // namespace tateyama::api::endpoint::ipc
