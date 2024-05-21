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

#include <map>
#include <vector>
#include <memory>

#include <tateyama/metrics/metrics_store.h>
#include <tateyama/metrics/resource/metrics_item_slot_impl.h>

#include <tateyama/proto/metrics/response.pb.h>

namespace tateyama::metrics::resource {

class core;

struct metadata_less {
    bool operator()(const tateyama::metrics::metrics_metadata& a, const tateyama::metrics::metrics_metadata& b) const noexcept {
        if (a.key() < b.key()) {
            return true;
        }
        auto& a_attrs = a.attributes();
        auto& b_attrs = b.attributes();
        if (a_attrs.size() != a_attrs.size()) {
            return a_attrs.size() < b_attrs.size();
        }
        for (std::size_t i = 0; i < a_attrs.size(); i++) {
            if (a_attrs.at(i) < b_attrs.at(i)) {
                return true;
            }
        }        
        return false;
    }
};

class metrics_store_impl {
    using second_map_type = std::map<tateyama::metrics::metrics_metadata, std::pair<tateyama::metrics::metrics_item_slot, std::shared_ptr<metrics_item_slot_impl>>, metadata_less>;

public:
    metrics_item_slot& register_item(const metrics_metadata& metadata);

    void register_aggregation(const metrics_aggregation& aggregation);

    bool unregister_element(std::string_view key);

    void enumerate_items(std::function<void(metrics_metadata const&, double)> const& acceptor);

    void enumerate_aggregations(std::function<void(metrics_aggregation const&)> const& acceptor) const;

private:
    std::map<std::string, std::unique_ptr<second_map_type>> metrics_{};
    std::map<std::string, std::unique_ptr<second_map_type>> invisible_metrics_{};
    std::map<std::string, metrics_aggregation> aggregations_{};

    void set_item_description(::tateyama::proto::metrics::response::MetricsInformation& information);

    void set_item_value(::tateyama::proto::metrics::response::MetricsInformation& information);

    friend class core;
};

}
