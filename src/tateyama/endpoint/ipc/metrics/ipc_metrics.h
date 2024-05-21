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
        void add(tateyama::metrics::metrics_metadata const& metadata, double value) override {
            if (metadata.key() == "ipc_session_count") {
                session_count_ = value;
            } else if (metadata.key() == "memory_usage_per_session") {
                memory_per_session_ = value;
            }
        }
        result_type aggregate() override {
            if(tateyama::metrics::service::ipc_correction) {
                return (session_count_ - 1.0) * memory_per_session_;
            }
            return session_count_ * memory_per_session_;
        }
      private:
        double memory_per_session_{};
        double session_count_{};
    };

  public:
    explicit ipc_metrics(tateyama::framework::environment& env)
        : metrics_store_(env.resource_repository().find<::tateyama::metrics::resource::bridge>()->metrics_store()),
          session_count_(metrics_store_.register_item(session_count_metadata_)),
          ipc_memory_usage_(metrics_store_.register_item(ipc_memory_usage_metada_))
    {
        metrics_store_.register_aggregation(tateyama::metrics::metrics_aggregation{"session_count", "number of active sessions", [](){return std::make_unique<session_count_aggregator>();}});
        metrics_store_.register_aggregation(tateyama::metrics::metrics_aggregation{"ipc_buffer_size", "allocated buffer size for all IPC sessions", [](){return std::make_unique<ipc_memory_aggregator>();}});
    }

  private:
    tateyama::metrics::metrics_store& metrics_store_;

    tateyama::metrics::metrics_metadata session_count_metadata_ {
        "ipc_session_count"s, "number of active ipc sessions"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {"session_count"s, "ipc_buffer_size"s},
        false
    };
    tateyama::metrics::metrics_metadata ipc_memory_usage_metada_ {
        "memory_usage_per_session"s, "memory usage per session"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {"ipc_buffer_size"s},
        false
    };

    // have to be placed after corresponding metrics_metadata definition
    tateyama::metrics::metrics_item_slot& session_count_;
    tateyama::metrics::metrics_item_slot& ipc_memory_usage_;

    std::atomic_long count_{};

    void memory_usage(std::size_t bytes) noexcept {
        ipc_memory_usage_ = static_cast<double>(bytes);
    }
    void increase() noexcept {
        count_++;
        session_count_ = static_cast<double>(count_.load());
    }
    void decrease() noexcept {
        count_--;
        session_count_ = static_cast<double>(count_.load());
    }
    
    friend class tateyama::endpoint::ipc::bootstrap::ipc_listener;
};

}
