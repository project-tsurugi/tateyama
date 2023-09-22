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
#include "stream_listener.h"

namespace tateyama::framework {

/**
 * @brief stream endpoint component
 */
class stream_endpoint : public endpoint {
public:

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "stream_endpoint";

    /**
     * @brief construct the object
     */
    stream_endpoint() = default;

    /**
     * @brief destruct the object
     */
    ~stream_endpoint() override {
        VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
    }

    stream_endpoint(stream_endpoint const& other) = delete;
    stream_endpoint& operator=(stream_endpoint const& other) = delete;
    stream_endpoint(stream_endpoint&& other) noexcept = delete;
    stream_endpoint& operator=(stream_endpoint&& other) noexcept = delete;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(environment& env) override {
        try {
            // create listener object
            listener_ = std::make_unique<tateyama::server::stream_listener>(
                env.configuration(),
                env.service_repository().find<framework::routing_service>()
            );
            return true;
        } catch (std::exception &ex) {
            LOG_LP(ERROR) << ex.what();
            return false;
        }
    }

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(environment&) override {
        listener_thread_ = std::thread(std::ref(*listener_));
        listener_->arrive_and_wait();
        return true;
    }

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(environment&) override {
        // For clean up, shutdown can be called multiple times with/without setup()/start().
        if(listener_thread_.joinable()) {
            if(listener_) {
                listener_->terminate();
            }
            listener_thread_.join();
        }
        listener_.reset();
        return true;
    }

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override {
        return component_label;
    }

private:
    std::unique_ptr<tateyama::server::stream_listener> listener_;
    std::thread listener_thread_;
};

}
