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
#include <stdexcept>

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
    metrics_item_slot& register_item(const metrics_metadata& metadata) {
        auto key = std::string(metadata.key());
        if (metrics_.find(key) == metrics_.end()) {
            metrics_.emplace(key, std::make_unique<second_map_type>());
        }
        auto fitr = metrics_.find(key);
        if (fitr == metrics_.end()) {
            throw std::runtime_error("illegal data structute");
        }
        auto& fmap = fitr->second;
        if (fmap->find(metadata) != fmap->end()) {
            throw std::runtime_error("already exist");
        }
        auto item = std::make_shared<metrics_item_slot_impl>();
        fmap->emplace(metadata, std::make_pair(tateyama::metrics::metrics_item_slot(item), item));
        return (fmap->find(metadata)->second).first;
    }

    void register_aggregation(std::unique_ptr<metrics_aggregation> aggregation) {
        std::string key{aggregation->group_key()};
        if (aggregations_.find(key) != aggregations_.end()) {
            throw std::runtime_error("aggregation is already registered");
        }
        aggregations_.emplace(key, std::move(aggregation));
    }

    bool unregister_element(std::string_view key) {
        std::string key_string{key};
        if (auto const itr = metrics_.find(key_string); itr != metrics_.end()) {
            metrics_.erase(itr);
            return true;
        }
        if (auto const itr = aggregations_.find(key_string); itr != aggregations_.end()) {
            aggregations_.erase(itr);
            return true;
        }
        return false;
    }

    void enumerate_items(std::function<void(metrics_metadata const&, double)> const& acceptor) {
        for (auto&& fmap: metrics_) {
            for (auto&& e: *fmap.second) {
                acceptor(e.first, (e.second).second->value());
            }
        }
    }

    void enumerate_aggregations(std::function<void(metrics_aggregation const&)> const& acceptor) const {
        for (auto&& e: aggregations_) {
            acceptor(*(e.second));
        }
    }

private:
    std::map<std::string, std::unique_ptr<second_map_type>> metrics_{}; 
    std::map<std::string, std::unique_ptr<metrics_aggregation>> aggregations_{};

    void set_item_description(::tateyama::proto::metrics::response::MetricsInformation& information) {
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
            item->set_description(std::string(a.second->description()));
        }
    }

    void set_item_value(::tateyama::proto::metrics::response::MetricsInformation& information) {
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
            item->set_description(std::string(a.second->description()));

            auto aggregator = a.second->create_aggregator();
            for (auto&& fmap: metrics_) {
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
        }
    }

    friend class core;
};

}
