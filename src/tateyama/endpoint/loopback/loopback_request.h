/*
 * Copyright 2019-2023 tsurugi project.
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

#include <tateyama/api/server/request.h>

namespace tateyama::common::loopback {

class loopback_request: public tateyama::api::server::request {
public:
    loopback_request(std::size_t session_id, std::size_t service_id, std::string_view payload) :
            session_id_(session_id), service_id_(service_id) {
        payload_ = payload;
    }

    [[nodiscard]] std::size_t session_id() const override {
        return session_id_;
    }

    [[nodiscard]] std::size_t service_id() const override {
        return service_id_;
    }

    [[nodiscard]] std::string_view payload() const override {
        return payload_;
    }

private:
    std::size_t session_id_;
    std::size_t service_id_;
    std::string payload_;
};

} // namespace tateyama::common::loopback
