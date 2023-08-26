/*
 * Copyright 2018-2020 tsurugi project.
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

#include <cstdint>
#include <variant>
#include <ios>
#include <functional>
#include <emmintrin.h>
#include <deque>

#include <glog/logging.h>
#include <boost/exception/all.hpp>
#include <takatori/util/exception.h>

#include <tateyama/common.h>
#include <tateyama/api/task_scheduler/context.h>
#include <tateyama/api/task_scheduler/impl/thread_control.h>
#include <tateyama/api/task_scheduler/task_scheduler_cfg.h>
#include <tateyama/api/task_scheduler/impl/utils.h>
#include <tateyama/utils/cache_align.h>

namespace tateyama::task_scheduler {

using api::task_scheduler::task_scheduler_cfg;

/**
 * @brief context object for conditional worker
 * @details worker holds static information (given on constructor) only and dynamic context is separated to this object
 */
class conditional_worker_context {
public:

    /**
     * @brief create new object
     */
    conditional_worker_context() = default;

    /**
     * @brief destruct the object
     */
    ~conditional_worker_context() = default;

    conditional_worker_context(conditional_worker_context const& other) = default;
    conditional_worker_context& operator=(conditional_worker_context const& other) = default;
    conditional_worker_context(conditional_worker_context&& other) noexcept = default;
    conditional_worker_context& operator=(conditional_worker_context&& other) noexcept = default;

    /**
     * @brief accessor to the thread control that runs conditional worker
     * @return thread control
     */
    [[nodiscard]] thread_control* thread() const noexcept {
        return thread_;
    }

    /**
     * @brief set the thread control that runs conditional worker
     */
    void thread(thread_control* arg) noexcept {
        thread_ = arg;
    }

private:
    thread_control* thread_{};
};


/**
 * @brief condition watcher worker object
 * @details this represents the worker logic running on watcher thread that processes conditional task queue
 * @note this object is just a logic object and doesn't hold dynamic state, so safely be copied into thread_control.
 */
template <class T>
class cache_align conditional_worker {
public:
    /**
     * @brief conditional task type
     */
    using conditional_task = T;

    /**
     * @brief create empty object
     */
    conditional_worker() = default;

    /**
     * @brief create new object
     * @param q reference to the conditional task queue
     * @param cfg the scheduler configuration information
     * @param initializer the function called on worker thread for initialization
     */
    explicit conditional_worker(
        basic_queue<conditional_task>& q,
        task_scheduler_cfg const* cfg = nullptr
    ) noexcept:
        cfg_(cfg),
        q_(std::addressof(q))
    {}

    /**
     * @brief initialize the worker
     * @param info thread information that runs this worker
     */
    void init(thread_initialization_info const& info, conditional_worker_context& ctx) {
        // reconstruct the queues so that they are on same numa node
        (*q_).reconstruct();
        ctx.thread(info.thread());
    }

    /**
     * @brief the condition watcher worker body
     */
    void operator()(conditional_worker_context& ctx) {
        conditional_task t{};
        std::deque<conditional_task> negatives{};
        while(q_->active()) {
            negatives.clear();
            while(q_->try_pop(t)) {
                if(execute_task(true, t)) {
//                    std::cerr << "check : true" << std::endl;
                    execute_task(false, t);
                    continue;
                }
//                std::cerr << "check : false" << std::endl;
                negatives.emplace_back(std::move(t));
            }
//            std::cerr << "negatives: " << negatives.size() << std::endl;
            if(negatives.empty()) {
                ctx.thread()->suspend();
                continue;
            }
            for(auto&& e : negatives) {
                q_->push(std::move(e));
            }
            ctx.thread()->suspend(std::chrono::microseconds{cfg_ ? cfg_->watcher_interval() : 0});
//            std::cerr << "suspend timed out" << std::endl;
        }
    }

private:
    task_scheduler_cfg const* cfg_{};
    basic_queue<conditional_task>* q_{};

    bool execute_task(bool check_condition, conditional_task& t) {
        bool ret{};
        try {
            // use try-catch to avoid server crash even on fatal internal error
            if(check_condition) {
                ret = t.check();
            } else {
                t();
            }
        } catch (boost::exception& e) {
            // currently find_trace() after catching std::exception doesn't work properly. So catch as boost exception. TODO
            LOG(ERROR) << "Unhandled boost exception caught.";
            LOG(ERROR) << boost::diagnostic_information(e);
        } catch (std::exception& e) {
            LOG(ERROR) << "Unhandled exception caught: " << e.what();
            if(auto* tr = takatori::util::find_trace(e); tr != nullptr) {
                LOG(ERROR) << *tr;
            }
        }
        return ret;
    }

};

}
