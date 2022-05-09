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

#include <memory>

#include <tateyama/framework/ids.h>
#include <tateyama/framework/resource.h>
#include <tateyama/api/task_scheduler/scheduler.h>

namespace tateyama::framework {

/**
 * @brief resource component
 */
template <class T>
class task_scheduler_resource : public resource {
public:
    static constexpr id_type tag = resource_id_task_scheduler;

    [[nodiscard]] id_type id() const noexcept override {
        return tag;
    }

    /**
     * @brief setup the component (the state will be `ready`)
     */
    void setup(environment&) override {
        // no-op
    }

    /**
     * @brief start the component (the state will be `activated`)
     */
    void start(environment& env) override {
        if(env.mode() == boot_mode::database_server) {
            scheduler_->start();
        }
    }

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    void shutdown(environment& env) override {
        if(env.mode() == boot_mode::database_server) {
            scheduler_->stop();
        }
    }

    /**
     * @brief accessor to the core object
     * @return the scheduler held by this object
     */
    [[nodiscard]] api::task_scheduler::scheduler<T>* core_object() const noexcept {
        return scheduler_.get();
    }

private:
    std::unique_ptr<api::task_scheduler::scheduler<T>> scheduler_{};
};

}

