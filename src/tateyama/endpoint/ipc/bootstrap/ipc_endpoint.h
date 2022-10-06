/*
 * Copyright 2018-2022 tsurugi project.
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

#include <tateyama/framework/endpoint.h>
#include <tateyama/framework/environment.h>
#include "ipc_listener.h"

namespace tateyama::framework {

/**
 * @brief ipc endpoint component
 */
class ipc_endpoint : public endpoint {
public:

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(environment& env) override {
        // create listener object
        listener_ = std::make_unique<tateyama::server::ipc_listener>(
            env.configuration(),
            env.service_repository().find<framework::routing_service>()
        );
        return true;
    }

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(environment&) override {
        listener_thread_ = std::thread(std::ref(*listener_));
        return true;
    }

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(environment&) override {
        if(listener_) {
            listener_->terminate();
            listener_.reset();
        }
        if(listener_thread_.joinable()) {
            listener_thread_.join();
        }
        return true;
    }

private:
    std::unique_ptr<tateyama::server::ipc_listener> listener_;
    std::thread listener_thread_;
};

}

