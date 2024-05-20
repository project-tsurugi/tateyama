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

#include "tateyama/metrics/resource/bridge.h"

namespace tateyama::endpoint::stream::bootstrap {
    class stream_listener;
}

namespace tateyama::endpoint::stream::metrics {
using namespace std::literals::string_literals;

/**
 * @brief an object in charge of metrics chores
 */
class stream_metrics {
  public:
    explicit stream_metrics(tateyama::framework::environment& env)
        : metrics_store_(env.resource_repository().find<::tateyama::metrics::resource::bridge>()->metrics_store()),
          session_count_(metrics_store_.register_item(session_count_metadata_))
    {
    }

  private:
    tateyama::metrics::metrics_store& metrics_store_;

    tateyama::metrics::metrics_metadata session_count_metadata_ {
        "stream_session_count"s, "number of active ipc sessions"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {"session_count"s},
        false
    };

    // have to be placed after corresponding metrics_metadata definition
    tateyama::metrics::metrics_item_slot& session_count_;

    std::atomic_long count_{0};

    void increase() noexcept {
        count_++;
        session_count_ = static_cast<double>(count_.load());
    }
    void decrease() noexcept {
        count_--;
        session_count_ = static_cast<double>(count_.load());
    }

    friend class tateyama::endpoint::stream::bootstrap::stream_listener;
};

}
