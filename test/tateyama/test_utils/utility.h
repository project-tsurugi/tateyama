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
#include <filesystem>

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
        std::string log_location = path() + "/log";
        dscfg->set("log_location", log_location);
        {
            std::error_code ec;
            std::filesystem::create_directory(std::filesystem::path(log_location), ec);
            BOOST_ASSERT(!ec);
        }

        auto* ipccfg = cfg.get_section("ipc_endpoint");
        BOOST_ASSERT(ipccfg != nullptr); //NOLINT
        database_name_ = boost::filesystem::unique_path().string();
        ipccfg->set("database_name", database_name_);

        auto* stmcfg = cfg.get_section("stream_endpoint");
        BOOST_ASSERT(stmcfg != nullptr); //NOLINT
        // use port randomly chosen from PID
        auto id = getpid() % 1000;
        stmcfg->set("port", std::to_string(12345+id));

        auto* brcfg = cfg.get_section("blob_relay");
        BOOST_ASSERT(brcfg != nullptr); //NOLINT
        std::string session_store = path() + "/session_store";
        brcfg->set("session_store", session_store);
        {
            std::error_code ec;
            std::filesystem::create_directory(std::filesystem::path(session_store), ec);
            BOOST_ASSERT(!ec);
        }
    }
    ~utility() {
        temporary_.clean();
    }

protected:
    temporary_folder temporary_{};
    std::string database_name_{};
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
        "enabled=false\n"
        "port=12345\n"
        "threads=104\n"
        "allow_blob_privileged=false\n"

    "[session]\n"
        "enable_timeout=true\n"
        "refresh_timeout=300\n"
        "max_refresh_timeout=86400\n"
        "zone_offset=\n"
        "authentication_timeout=0\n"

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
        "administrators=*\n"

    "[grpc_server]\n"
        "enabled=true\n"
        "listen_address=0.0.0.0:52345\n"
        "endpoint=dns:///localhost:52345\n"
        "secure=false\n"

    "[blob_relay]\n"
        "enabled=true\n"
        "session_store=var/blob/sessions\n"
        "session_quota_size=\n"
        "local_enabled=true\n"
        "local_upload_copy_file=false\n"
        "stream_chunk_size=1048576\n"

    "[glog]\n"
        "dummy=\n"  // just for retain glog section in default configuration
};

}
