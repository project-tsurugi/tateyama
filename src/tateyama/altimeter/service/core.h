/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/service.h>

#include <tateyama/proto/altimeter/response.pb.h>

namespace tateyama::altimeter::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
 * @brief altimeter helper object
 */
class altimeter_helper {
public:
    altimeter_helper() = default;
    virtual ~altimeter_helper() = default;

    altimeter_helper(altimeter_helper const& other) = delete;
    altimeter_helper& operator=(altimeter_helper const& other) = delete;
    altimeter_helper(altimeter_helper&& other) noexcept = delete;
    altimeter_helper& operator=(altimeter_helper&& other) noexcept = delete;

    virtual void set_enabled(std::string_view, const bool&)= 0;
    virtual void set_level(std::string_view, std::uint64_t) = 0;
    virtual void set_stmt_duration_threshold(std::uint64_t) = 0;
    virtual void rotate_all(std::string_view) = 0;
};

/**
 * @brief altimeter service main object
 */
class core {
public:
    core();

    bool operator()(
        const std::shared_ptr<request>& req,
        const std::shared_ptr<response>& res
    );

private:
    std::shared_ptr<altimeter_helper> helper_;

    void replace_helper(std::shared_ptr<altimeter_helper> helper) {
        helper_ = std::move(helper);
    }

    template<typename T>
    void send_error(
        const std::shared_ptr<response>& res,
        tateyama::proto::altimeter::response::ErrorKind error_kind,
        const std::string& error_message
    ) {
        T rs{};
        auto* error = rs.mutable_error();
        error->set_kind(error_kind);
        error->set_message(error_message);
        res->body(rs.SerializeAsString());
        rs.clear_error();
    }

    friend class bridge;
};

}
