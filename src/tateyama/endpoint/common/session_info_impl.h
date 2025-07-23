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

#include <sstream>

#include <tateyama/api/server/session_info.h>
#include "administrators.h"

namespace tateyama::endpoint::common {

class worker_common;

/**
 * @brief session_info_impl
 */
class session_info_impl : public tateyama::api::server::session_info {
public:
    session_info_impl(std::size_t id, std::string_view con_type, std::string_view con_info, const tateyama::endpoint::common::administrators& administrators)
        : id_(id), connection_type_name_(con_type), connection_information_(con_info), administrators_(administrators) {
    }

    [[nodiscard]] id_type id() const noexcept override { return id_; }

    [[nodiscard]] std::string_view label() const noexcept override { return connection_label_; }

    [[nodiscard]] std::string_view application_name() const noexcept override { return application_name_; }

    [[nodiscard]] time_type start_at() const noexcept override { return start_at_; }

    [[nodiscard]] std::string_view connection_type_name() const noexcept override { return connection_type_name_; }

    [[nodiscard]] std::string_view connection_information() const noexcept override { return connection_information_; }

    [[nodiscard]] std::optional<std::string_view> username() const noexcept override { return user_name_opt_; }

    [[nodiscard]] tateyama::api::server::user_type user_type() const noexcept override {
        if (user_name_opt_) {
            return administrators_.is_administrator(user_name_opt_.value()) ? tateyama::api::server::user_type::administrator : tateyama::api::server::user_type::standard;
        }
        return tateyama::api::server::user_type::administrator;
    }

private:
    // server internal info.
    const id_type id_;

    const std::string connection_type_name_;

    const time_type start_at_{std::chrono::system_clock::now()};

    std::string connection_information_;

    const administrators& administrators_;

    // provided by the client
    std::string connection_label_{};

    std::string application_name_{};

    std::string user_name_{};
    std::optional<std::string_view> user_name_opt_{std::nullopt};

protected:
    void label(std::string_view str) {
        connection_label_ = str;
    }
    void application_name(std::string_view name) {
        application_name_ = name;
    }
    void connection_information(std::string_view info) {
        connection_information_ = info;
    }
    void user_name(const std::string& name) {
        user_name_ = name;
        user_name_opt_ = user_name_;
    }

    friend class worker_common;
};

inline std::ostream& operator<<(std::ostream& out, const session_info_impl& info) {
    std::stringstream ss;
    auto name_opt = info.username();
    ss << "session_info["
       << static_cast<std::size_t>(info.id()) << ","
       << info.connection_type_name() << ", "
       << info.connection_information() << ": "
       << info.label() << ","
       << info.application_name() << ","
       << (name_opt ? name_opt.value() : "")
       << "]";
    return out << ss.str();
}

}
