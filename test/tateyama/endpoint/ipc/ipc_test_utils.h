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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <tateyama/datastore/service/bridge.h>

#include <tateyama/framework/server.h>
#include <tateyama/framework/routing_service.h>

#include <tateyama/endpoint/ipc/bootstrap/server_wires_impl.h>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/endpoint/header_utils.h>
#include <tateyama/utils/test_utils.h>

#include <tateyama/endpoint/common/endpoint_proto_utils.h>
#include <tateyama/endpoint/ipc/ipc_request.h>
#include <tateyama/endpoint/ipc/ipc_response.h>

namespace tateyama::api::endpoint::ipc {

void get_ipc_database_name(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
        std::string &ipc_database_name);
void get_ipc_max_session(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, std::size_t &max_session);

void make_power2_length_list(std::vector<std::size_t> &vec, std::size_t max);
void dump_length_list(std::vector<std::size_t> &vec);

void make_dummy_message(std::size_t start_idx, std::size_t len, std::string &message);
bool check_dummy_message(std::size_t start_idx, std::string &message);
void make_printable_dummy_message(std::size_t start_idx, std::size_t len, std::string &message);

class elapse {
public:
    void start() {
        start_ = end_ = now();
    }
    void stop() {
        end_ = now();
    }
    std::chrono::milliseconds now() {
        return std::chrono::duration_cast < std::chrono::milliseconds
                > (std::chrono::system_clock::now().time_since_epoch());
    }
    std::int64_t msec() {
        if (end_ >= start_) {
            return static_cast<std::int64_t>((end_ - start_).count());
        } else {
            return -1;
        }
    }
private:
    std::chrono::milliseconds start_ { }, end_ { };
};

class server_service_base: public tateyama::framework::service {
public:
    static constexpr tateyama::framework::component::id_type tag = 1000;
    [[nodiscard]] virtual tateyama::framework::component::id_type id() const noexcept override {
        return tag;
    }

    virtual bool start(tateyama::framework::environment&) override {
        return true;
    }
    virtual bool setup(tateyama::framework::environment&) override {
        return true;
    }
    virtual bool shutdown(tateyama::framework::environment&) override {
        return true;
    }

    virtual bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
            std::shared_ptr<tateyama::api::server::response> res) = 0;
};

class ipc_test_base: public ::testing::Test, public test::test_utils {
    void SetUp() override {
        temporary_.prepare();
        cfg_ = tateyama::api::configuration::create_configuration("");
        set_dbpath(*cfg_);
        get_ipc_max_session(cfg_, ipc_max_session_);
    }
    void TearDown() override {
        temporary_.clean();
    }

protected:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_ { };
    std::size_t ipc_max_session_ { };
};

} // namespace tateyama::api::endpoint::ipc

