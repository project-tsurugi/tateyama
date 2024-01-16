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
#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

#include <tateyama/endpoint/header_utils.h>
#include <tateyama/utils/test_utils.h>

#include <tateyama/endpoint/common/endpoint_proto_utils.h>
#include <tateyama/endpoint/ipc/ipc_request.h>
#include <tateyama/endpoint/ipc/ipc_response.h>

namespace tateyama::endpoint::ipc {

void get_ipc_database_name(std::shared_ptr<tateyama::api::configuration::whole> const &cfg,
        std::string &ipc_database_name);
void get_ipc_max_session(std::shared_ptr<tateyama::api::configuration::whole> const &cfg, int &max_session);

void make_power2_length_list(std::vector<std::size_t> &vec, const std::size_t max);
void dump_length_list(std::vector<std::size_t> &vec);

void make_dummy_message(const std::size_t start_idx, const std::size_t len, std::string &message);
bool check_dummy_message(const std::size_t start_idx, const std::string_view message);

void params_to_string(const std::size_t len, const std::vector<std::size_t> &msg_params, std::string &text);
std::size_t get_message_len(const std::string_view message);
std::string_view get_message_part(const std::string_view message);
std::string_view get_message_part_without_len(const std::string_view message);
void make_dummy_message(const std::string &part, const std::size_t len, std::string &message);
bool check_dummy_message(const std::string_view message);

inline void assert_failed() {
    std::cout << boost::stacktrace::stacktrace();
    std::exit(1);
}

inline void assert_true(bool result) {
    if (!result) {
        assert_failed();
    }
}

inline void assert_eq(int v1, int v2) {
    if (v1 != v2) {
        assert_failed();
    }
}

inline void assert_gt(int v1, int v2) {
    if (v1 <= v2) {
        assert_failed();
    }
}

class elapse {
public:
    void start() noexcept {
        start_ = end_ = now_nanosec();
    }
    void stop() noexcept {
        end_ = now_nanosec();
    }
    std::chrono::milliseconds now_msec() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());
    }
    std::chrono::nanoseconds now_nanosec() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    }
    std::int64_t nanosec() noexcept {
        if (end_ >= start_) {
            return static_cast<std::int64_t>((end_ - start_).count());
        } else {
            return -1;
        }
    }
    std::int64_t msec() noexcept {
        return nanosec() / 1'000'000;
    }
    double sec() noexcept {
        return 1e-9 * nanosec();
    }
private:
    std::chrono::nanoseconds start_ { }, end_ { };
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

    [[nodiscard]] std::string_view label() const noexcept override {
        return "server_service_base";
    }
};

class resultset_param {
public:
    resultset_param(const std::string &name, std::vector<std::size_t> &write_lens, std::size_t write_nloop,
            std::size_t value = 1) :
            name_(name), write_nloop_(write_nloop), write_lens_(write_lens), value_(value) {
    }
    resultset_param(const std::string &text);
    void to_string(std::string &text) noexcept;

    std::string name_ { };
    std::size_t write_nloop_ { };
    std::size_t value_ { };
    std::vector<std::size_t> write_lens_ { };
private:
    static constexpr char delim = ',';
};

class msg_info {
private:
    std::vector<std::size_t> params { };
public:
    msg_info(const std::size_t session_id, const std::size_t idx) noexcept {
        params.push_back(session_id);
        params.push_back(idx);
        params.push_back(0);
    }

    msg_info(const std::shared_ptr<tateyama::api::server::request> &req, const std::size_t idx) noexcept :
            msg_info(req->session_id(), idx) {
    }

    void set_index(const std::size_t sub_idx) noexcept {
        params[2] = sub_idx;
    }

    void to_string(const std::size_t len, std::string &text) noexcept {
        return params_to_string(len, params, text);
    }
};

} // namespace tateyama::endpoint::ipc

