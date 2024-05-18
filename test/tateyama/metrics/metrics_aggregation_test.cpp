/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <tateyama/metrics/metrics_metadata.h>
#include <tateyama/metrics/metrics_aggregation.h>
#include <tateyama/metrics/resource/metrics_store_impl.h>

#include <tateyama/metrics/resource/bridge.h>

#include <gtest/gtest.h>
#include <tateyama/utils/test_utils.h>

namespace tateyama::metrics {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

// example aggregator
class aggregator_for_aggregation_test : public metrics_aggregator {
public:
    void add(metrics_metadata const& metadata, double value) override {
        value_ += 1.0;
    }
    result_type aggregate() override {
        return value_;
    }
private:
    double value_{};
};

// example aggregation
class aggregation_for_aggregation_test : public metrics_aggregation {
public:
    aggregation_for_aggregation_test(
        const std::string& group_key,
        const std::string& description) :
        metrics_aggregation(group_key, description) {
    }
    std::unique_ptr<metrics_aggregator> create_aggregator() const noexcept override {
        return std::make_unique<aggregator_for_aggregation_test>();
    }
};


class metrics_aggregation_test :
    public ::testing::Test,
    public test::test_utils
{
public:
    void SetUp() override {
        auto store = std::make_unique<resource::metrics_store_impl>();
        metrics_store_impl_ = store.get();
        metrics_store_ = std::make_unique<metrics_store>(std::move(store));
    }
    void TearDown() override {
    }

private:
    resource::metrics_store_impl* metrics_store_impl_{};
    
protected:
    std::unique_ptr<metrics_store> metrics_store_{};
    
    metrics_metadata const metadata_table_A1_{
        "index_size"s, "estimated each index size"s,
        std::vector<std::tuple<std::string, std::string>> {
            std::tuple<std::string, std::string> {"table_name"s, "A"s},
            std::tuple<std::string, std::string> {"index_name"s, "IA1"}
        },
        std::vector<std::string> {"table_count"s}
    };
    metrics_metadata const metadata_table_A2_{
        "index_size"s, "estimated each index size"s,
        std::vector<std::tuple<std::string, std::string>> {
            std::tuple<std::string, std::string> {"table_name"s, "A"s},
            std::tuple<std::string, std::string> {"index_name"s, "IA2"}
        },
        std::vector<std::string> {"table_count"s}
    };
    metrics_metadata const metadata_table_B_{
        "index_size"s, "estimated each index size"s,
        std::vector<std::tuple<std::string, std::string>> {
            std::tuple<std::string, std::string> {"table_name"s, "B"s},
            std::tuple<std::string, std::string> {"index_name"s, "IB1"}
        },
        std::vector<std::string> {"table_count"s}
    };
};

TEST_F(metrics_aggregation_test, basic) {
    auto& slot_A1 = metrics_store_->register_item(metadata_table_A1_);
    auto& slot_A2 = metrics_store_->register_item(metadata_table_A2_);
    auto& slot_B = metrics_store_->register_item(metadata_table_B_);
    metrics_store_->register_aggregation(std::make_unique<aggregation_for_aggregation_test>("table_count", "number of user tables"));

    slot_A1 = 65536;
    slot_A2 = 16777216;
    slot_B = 256;

    metrics_store_->enumerate_items([](metrics_metadata const& m, double v) {
        EXPECT_EQ(m.key(), "index_size");
        auto attrs = m.attributes();
        EXPECT_EQ(std::get<0>(attrs.at(0)), "table_name");
        EXPECT_EQ(std::get<0>(attrs.at(1)), "index_name");

        if (std::get<1>(attrs.at(0)) == "A") {
            if (std::get<1>(attrs.at(1)) == "IA1") {
                EXPECT_EQ(65536, v);
            } else if (std::get<1>(attrs.at(1)) == "IA2") {
                EXPECT_EQ(16777216, v);
            } else {
                FAIL();
            }
        } else if (std::get<1>(attrs.at(0)) == "B") {
            EXPECT_EQ(std::get<1>(attrs.at(1)), "IB1");
            EXPECT_EQ(256, v);
        } else {
            FAIL();
        }
    });

    metrics_store_->enumerate_aggregations([this](metrics_aggregation const& a) {
        auto aggregator = a.create_aggregator();
        metrics_store_->enumerate_items([&a, &aggregator](metrics_metadata const& m, double v) {
            aggregator->add(m, v);
        });
        auto r = aggregator->aggregate();
        if (double* p = std::get_if<0>(&r); p) {
            EXPECT_EQ(3., *p);
        } else {
            FAIL();
        }
    });
}

}
