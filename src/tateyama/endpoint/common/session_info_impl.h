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

#include <tateyama/api/server/session_info.h>

namespace tateyama::endpoint::common {

/**
 * @brief session_info_impl
 */
class session_info_impl : public tateyama::api::server::session_info {
public:
    session_info_impl(std::size_t id, std::string_view con_type, std::string_view con_info)
        : id_(id), connection_type_name_(con_type), connection_information_(con_info) {
    }
    session_info_impl() : session_info_impl(0, "test_purpose", "test_purpose") {
    }

    id_type id() const noexcept { return id_; }

    std::string_view label() const noexcept{ return label_; }

    std::string_view application_name() const noexcept{ return application_name_; }

    std::string_view user_name() const noexcept{ return user_name_; }

    time_type start_at() const noexcept{ return start_at_; }

    std::string_view connection_type_name() const noexcept{ return connection_type_name_; }

    std::string_view connection_information() const noexcept{ return connection_information_; }

private:
    // server internal info.
    const id_type id_;

    const std::string connection_type_name_;

    const time_type start_at_{std::chrono::system_clock::now()};

    std::string connection_information_;

    // provided by the client
    std::string label_{};

    std::string application_name_{};

    std::string user_name_{};
};

}
