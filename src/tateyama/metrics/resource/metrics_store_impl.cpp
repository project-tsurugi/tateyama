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

#include <functional>
#include <stdexcept>

#include "metrics_store_impl.h"

namespace tateyama::metrics::resource {

metrics_item_slot& metrics_store_impl::register_item(const metrics_metadata& metadata) {
    const std::string key{metadata.key()};
    const auto register_item_internal = [&metadata, &key](std::map<std::string, std::unique_ptr<second_map_type>>& metrics) {
        if (metrics.find(key) == metrics.end()) {
            metrics.emplace(key, std::make_unique<second_map_type>());
        }
        auto fitr = metrics.find(key);
        if (fitr == metrics.end()) {
            throw std::runtime_error("illegal data structute");
        }
        auto& fmap = fitr->second;
        if (fmap->find(metadata) != fmap->end()) {
            throw std::runtime_error("already exist");
        }
        auto item = std::make_shared<metrics_item_slot_impl>();
        fmap->emplace(metadata, std::make_pair(tateyama::metrics::metrics_item_slot(item), item));
    };
    if (metadata.is_visible()) {
        register_item_internal(metrics_);
        return (metrics_.find(key)->second->find(metadata)->second).first;
    }
    register_item_internal(invisible_metrics_);
    return (invisible_metrics_.find(key)->second->find(metadata)->second).first;
}

void metrics_store_impl::register_aggregation(const metrics_aggregation& aggregation) {
    const std::string key{aggregation.group_key()};
    if (aggregations_.find(key) != aggregations_.end()) {
        throw std::runtime_error("aggregation is already registered");
    }
    aggregations_.emplace(key, aggregation);
}

bool metrics_store_impl::unregister_element(std::string_view key) {
    const std::string key_string{key};
    const auto unregister_element_internal = [&key_string](std::map<std::string, std::unique_ptr<second_map_type>>& metrics) {
        if (auto const itr = metrics.find(key_string); itr != metrics.end()) {
            metrics.erase(itr);
            return true;
        }
        return false;
    };
    if (unregister_element_internal(metrics_)) {
        return true;
    }
    if (unregister_element_internal(invisible_metrics_)) {
        return true;
    }
    if (auto const itr = aggregations_.find(key_string); itr != aggregations_.end()) {
        aggregations_.erase(itr);
        return true;
    }
    return false;
}

void metrics_store_impl::enumerate_items(std::function<void(metrics_metadata const&, double)> const& acceptor) {
    const auto enumerate_items_internal = [&acceptor](std::map<std::string, std::unique_ptr<second_map_type>>& metrics) {
        for (auto&& fmap: metrics) {
            for (auto&& e: *fmap.second) {
                acceptor(e.first, (e.second).second->value());
            }
        }
    };
    enumerate_items_internal(metrics_);
    enumerate_items_internal(invisible_metrics_);
}

void metrics_store_impl::enumerate_aggregations(std::function<void(metrics_aggregation const&)> const& acceptor) const {
    for (auto&& e: aggregations_) {
        acceptor(e.second);
    }
}

void metrics_store_impl::set_item_description(::tateyama::proto::metrics::response::MetricsInformation& information) {
    for (auto&& ef: metrics_) {
        auto* item = information.add_items();
        item->set_key(ef.first);
        auto&& map = ef.second;
        for (auto&& es: *map) {
            item->set_description(std::string(es.first.description()));
            break;
        }
    }
    for (auto&& a: aggregations_) {
        auto* item = information.add_items();
        item->set_key(a.first);
        item->set_description(std::string(a.second.description()));
    }
}

void metrics_store_impl::set_item_value(::tateyama::proto::metrics::response::MetricsInformation& information) {
    class encoder {
    public:
        explicit encoder(::tateyama::proto::metrics::response::MetricsValue* value) : value_(value) {
        }
        void operator()(const double v) {
            value_->set_value(v);
        }
        void operator()(const std::vector<metrics_element>& v) {
            auto* me = value_->mutable_array()->mutable_elements();
            for (auto&& ve: v) {
                auto* element = me->Add();
                element->set_value(ve.value());
                auto* mattrmap = element->mutable_attributes();
                for (auto&& vattr: ve.attributes()) {
                    (*mattrmap)[std::get<0>(vattr)] = std::get<1>(vattr);
                }
            }
        }
    private:
        ::tateyama::proto::metrics::response::MetricsValue* value_;
    };
    
    for (auto&& ef: metrics_) {
        auto* item = information.add_items();
        item->set_key(ef.first);
        auto&& map = ef.second;
        for (auto&& es: *map) {
            auto&& md = es.first;
            item->set_description(std::string(md.description()));
            auto* mv = item->mutable_value();
            auto& mattrs = md.attributes();
            if (mattrs.empty()) {
                mv->set_value((es.second).second->value());
            } else {
                auto* array = mv->mutable_array();
                auto* elements = array->mutable_elements();
                auto* element = elements->Add();
                element->set_value((es.second).second->value());
                auto* attrmap = element->mutable_attributes();
                for (auto&& e: mattrs) {
                    (*attrmap)[std::get<0>(e)] = std::get<1>(e);
                }
            }
        }
    }
    
    for (auto&& a: aggregations_) {
        auto* item = information.add_items();
        item->set_key(a.first);
        item->set_description(std::string(a.second.description()));
        
        auto aggregator = a.second.create_aggregator();
        const auto aggregate_internal = [&aggregator, &a, item](std::map<std::string, std::unique_ptr<second_map_type>>& metrics) {
            for (auto&& fmap: metrics) {
                for (auto&& e: *fmap.second) {
                    for (auto&& gk: e.first.group_keys()) {
                        if (a.first == gk) {
                            aggregator->add(e.first, (e.second).second->value());
                            break;
                        }
                    }
                }
            }
            std::visit(encoder(item->mutable_value()), aggregator->aggregate());
        };
        aggregate_internal(metrics_);
        aggregate_internal(invisible_metrics_);
    }
}

}
