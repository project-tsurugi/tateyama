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
    class session_count_aggregation : public tateyama::metrics::metrics_aggregation {
    public:
        session_count_aggregation(
            const std::string& group_key,
            const std::string& description) : metrics_aggregation(group_key, description) {
        }
        [[nodiscard]] std::unique_ptr<tateyama::metrics::metrics_aggregator> create_aggregator() const noexcept override {
            return std::make_unique<session_count_aggregator>();
        }
    };
    // for ipc_memory
    class ipc_memory_aggregator : public tateyama::metrics::metrics_aggregator {
    public:
        ipc_memory_aggregator() = delete;
        explicit ipc_memory_aggregator(std::size_t bytes) : memory_per_session_(bytes) {
        }
        void add(tateyama::metrics::metrics_metadata const&, double value) override {
            value_ += value;
        }
        result_type aggregate() override {
            if(tateyama::metrics::service::ipc_correction) {
                return (value_ - 1.0) * static_cast<double>(memory_per_session_);
            }
            return value_ * static_cast<double>(memory_per_session_);
        }
      private:
        std::size_t memory_per_session_{};
        double value_{};
    };
    class ipc_memory_aggregation : public tateyama::metrics::metrics_aggregation {
    public:
        ipc_memory_aggregation(
            const std::string& group_key,
            const std::string& description) : metrics_aggregation(group_key, description) {
        }
        void set_memory_per_session_(std::size_t bytes) noexcept {
            memory_per_session_ = bytes;
        }
        [[nodiscard]] std::unique_ptr<tateyama::metrics::metrics_aggregator> create_aggregator() const noexcept override {
            return std::make_unique<ipc_memory_aggregator>(memory_per_session_);
        }
    private:
        std::size_t memory_per_session_{};
    };

  public:
    explicit ipc_metrics(tateyama::framework::environment& env)
        : metrics_store_(env.resource_repository().find<::tateyama::metrics::resource::bridge>()->metrics_store()),
          session_count_(metrics_store_.register_item(session_count_metadata_))
    {
        metrics_store_.register_aggregation(std::make_unique<session_count_aggregation>("session_count", "number of active sessions"));
        auto memory_aggregation = std::make_unique<ipc_memory_aggregation>("ipc_buffer_size", "allocated buffer size for all IPC sessions");
        ipc_memory_aggregation_ = memory_aggregation.get();
        metrics_store_.register_aggregation(std::move(memory_aggregation));
    }

  private:
    tateyama::metrics::metrics_store& metrics_store_;

    tateyama::metrics::metrics_metadata session_count_metadata_ {
        "ipc_session_count"s, "number of active ipc sessions"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {"session_count"s, "ipc_buffer_size"s},
        false
    };

    // have to be placed after corresponding metrics_metadata definition
    tateyama::metrics::metrics_item_slot& session_count_;
    ipc_memory_aggregation *ipc_memory_aggregation_{};

    std::atomic_long count_{};

    void memory_usage(std::size_t bytes) noexcept {
        if (ipc_memory_aggregation_) {
            ipc_memory_aggregation_->set_memory_per_session_(bytes);
        }
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
