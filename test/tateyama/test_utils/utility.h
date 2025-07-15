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

#include <sys/types.h>
#include <unistd.h>

#include "temporary_folder.h"
#include "request_response.h"
#include "test_server.h"

namespace tateyama::test_utils {

class utility {
public:
    [[nodiscard]] std::string path() const {
        return temporary_.path();
    }

    void set_dbpath(api::configuration::whole& cfg) {
        auto* dscfg = cfg.get_section("datastore");
        BOOST_ASSERT(dscfg != nullptr); //NOLINT
        dscfg->set("log_location", path());

        auto* ipccfg = cfg.get_section("ipc_endpoint");
        BOOST_ASSERT(ipccfg != nullptr); //NOLINT
        auto p = boost::filesystem::unique_path();
        ipccfg->set("database_name", p.string());

        auto* stmcfg = cfg.get_section("stream_endpoint");
        BOOST_ASSERT(stmcfg != nullptr); //NOLINT
        // use port randomly chosen from PID
        auto id = getpid() % 1000;
        stmcfg->set("port", std::to_string(12345+id));
    }

protected:
    temporary_folder temporary_{};
};

static constexpr std::string_view default_configuration_for_tests {  // NOLINT
    "[sql]\n"
        "thread_pool_size=5\n"
        "enable_index_join=false\n"
        "stealing_enabled=true\n"
        "default_partitions=5\n"
        "stealing_wait=1\n"
        "task_polling_wait=0\n"
        "lightweight_job_level=0\n"
        "enable_hybrid_scheduler=true\n"

    "[ipc_endpoint]\n"
        "database_name=tsurugi\n"
        "threads=104\n"
        "datachannel_buffer_size=64\n"
        "max_datachannel_buffers=16\n"
        "admin_sessions=1\n"
        "allow_blob_privileged=true\n"

    "[stream_endpoint]\n"
        "enabled=true\n"
        "port=12345\n"
        "threads=104\n"
        "allow_blob_privileged=false\n"

    "[session]\n"
        "enable_timeout=true\n"
        "refresh_timeout=300\n"
        "max_refresh_timeout=86400\n"
        "zone_offset=\n"

    "[datastore]\n"
        "log_location=\n"
        "logging_max_parallelism=112\n"
        "recover_max_parallelism=8\n"

    "[cc]\n"
        "epoch_duration=40000\n"
        "waiting_resolver_threads=2\n"
        "max_concurrent_transactions=\n"

    "[system]\n"
        "pid_directory = /tmp\n"

    "[authentication]\n"
        "enabled=false\n"
        "url=http://localhost:8080/harinoki\n"
        "request_timeout=0\n"
        "administrators=\n"

    "[glog]\n"
        "dummy=\n"  // just for retain glog section in default configuration
};

}
