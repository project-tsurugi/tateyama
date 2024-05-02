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

#include "metrics_store_impl.h"

#include <tateyama/proto/metrics/response.pb.h>

namespace tateyama::metrics::resource {

class bridge;

/**
 * @brief the core class of `metrics` resource that provides information about metrics.
 */
class core {
public:
    /**
     * @brief construct the object
     */
    core() = default;
    
    /**
     * @brief destruct the object
     */
    virtual ~core() = default;
    
    core(core const&) = delete;
    core(core&&) = delete;
    core& operator = (core const&) = delete;
    core& operator = (core&&) = delete;
    
    /**
     * @brief handle list command
     */
    void list(::tateyama::proto::metrics::response::MetricsInformation&) const;

    /**
     * @brief handle show command
     */
    void show(::tateyama::proto::metrics::response::MetricsInformation&) const;

private:
    metrics_store_impl* metrics_store_impl_{};

    friend class bridge;
};

}
