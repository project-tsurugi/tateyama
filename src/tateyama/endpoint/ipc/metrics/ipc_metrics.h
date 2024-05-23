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

#include <atomic>

#include <tateyama/framework/resource.h>
#include <tateyama/framework/environment.h>
#include <tateyama/metrics/metrics_store.h>

#include "tateyama/metrics/service/core.h"
#include "tateyama/metrics/resource/bridge.h"

namespace tateyama::endpoint::ipc::bootstrap {
    class ipc_listener;
}

namespace tateyama::endpoint::ipc::metrics {
using namespace std::literals::string_literals;

/**
 * @brief an object in charge of metrics chores
 */
class ipc_metrics {
    // for session_count
    class session_count_aggregator : public tateyama::metrics::metrics_aggregator {
    public:
        void add(tateyama::metrics::metrics_metadata const&, double value) override {
            value_ += value;
        }
        result_type aggregate() override {
            return value_ - 1.0;
        }
      private:
        double value_{};
    };
    // for ipc_memory
    class ipc_memory_aggregator : public tateyama::metrics::metrics_aggregator {
    public:
        ipc_memory_aggregator(std::size_t fixed, std::size_t proportional)
            : fixed_(static_cast<double>(fixed)), proportional_(static_cast<double>(proportional)) {
        }
        ipc_memory_aggregator() = delete;
        void add(tateyama::metrics::metrics_metadata const&, double value) override {
            ipc_session_count_ = value;
        }
        result_type aggregate() override {
            if(tateyama::metrics::service::ipc_correction) {
                return fixed_ + (ipc_session_count_ - 1.0) * proportional_;
            }
            return fixed_ + ipc_session_count_ * proportional_;
        }
      private:
        double fixed_;
        double proportional_{};
        double ipc_session_count_{};
    };

  public:
    explicit ipc_metrics(tateyama::framework::environment& env)
        : metrics_store_(env.resource_repository().find<::tateyama::metrics::resource::bridge>()->metrics_store()),
          session_count_slot_(metrics_store_.register_item(tateyama::metrics::metrics_metadata{"ipc_session_count"s, "number of active ipc sessions"s,
                                                                                          std::vector<std::tuple<std::string, std::string>> {},
                                                                                          std::vector<std::string> {"session_count"s, "ipc_buffer_size"s},
                                                                                          false})) {
            metrics_store_.register_aggregation(tateyama::metrics::metrics_aggregation{"session_count", "number of active sessions", [](){return std::make_unique<session_count_aggregator>();}});
        }

  private:
    tateyama::metrics::metrics_store& metrics_store_;
    tateyama::metrics::metrics_item_slot& session_count_slot_;

    std::atomic_long session_count_{};

    void set_memory_parameters(std::size_t f, std::size_t p) noexcept {
        metrics_store_.register_aggregation(tateyama::metrics::metrics_aggregation{"ipc_buffer_size",
                                                                                   "allocated buffer size for all IPC sessions",
                                                                                   [f, p](){return std::make_unique<ipc_memory_aggregator>(f, p);}});
    }
    void increase() noexcept {
        session_count_++;
        session_count_slot_ = static_cast<double>(session_count_.load());
    }
    void decrease() noexcept {
        session_count_--;
        session_count_slot_ = static_cast<double>(session_count_.load());
    }
    
    friend class tateyama::endpoint::ipc::bootstrap::ipc_listener;
};

}
