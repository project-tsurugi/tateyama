/*
 * Copyright 2018-2021 tsurugi project.
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

#include <tateyama/status.h>
#include <tateyama/api/environment.h>
#include <tateyama/framework/endpoint_broker.h>

namespace tateyama::api::server {
class service;
}

namespace tateyama::api::endpoint {

class request;
class response;

/**
 * @brief tateyama service interface
 * @details this object provides access to send request and receive response to/from tateyama server application
 */
class service : public tateyama::framework::endpoint_broker {
public:
    /**
     * @brief create empty object
     */
    service() = default;

    /**
     * @brief destruct the object
     */
    ~service() override = default;

    service(service const& other) = delete;
    service& operator=(service const& other) = delete;
    service(service&& other) noexcept = delete;
    service& operator=(service&& other) noexcept = delete;

    bool start(framework::environment&) override {
        return true;
    }

    bool shutdown(framework::environment&) override {
        return true;
    }
};

}
