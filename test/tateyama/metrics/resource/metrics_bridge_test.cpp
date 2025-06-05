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
#include <tateyama/metrics/resource/bridge.h>

#include <tateyama/framework/server.h>

#include <tateyama/proto/metrics/request.pb.h>
#include <tateyama/proto/metrics/response.pb.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::metrics {

using namespace std::literals::string_literals;

// example aggregator
class aggregator_for_bridge_test : public metrics_aggregator {
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

class metrics_bridge_test :
        public ::testing::Test,
        public test_utils::utility {
public:
    void SetUp() override {
        temporary_.prepare();
        auto cfg = tateyama::api::configuration::create_configuration("", tateyama::test_utils::default_configuration_for_tests);
        set_dbpath(*cfg);
        sv_ = std::make_unique<framework::server>(tateyama::framework::boot_mode::database_server, cfg);
        tateyama::test_utils::add_core_components_for_test(*sv_);
        sv_->start();
        bridge_ = sv_->find_resource<tateyama::metrics::resource::bridge>();
    }
    void TearDown() override {
        sv_->shutdown();
        temporary_.clean();
    }

protected:
    std::unique_ptr<framework::server> sv_{};
    std::shared_ptr<metrics::resource::bridge> bridge_{};

    std::vector<std::tuple<std::string, std::string>> session_count_attributes_{};
    std::vector<std::string> session_count_group_keys_{};
    std::vector<std::tuple<std::string, std::string>> storage_log_size_attributes_{};
    std::vector<std::string> storage_log_size_group_keys_{};
    std::vector<std::tuple<std::string, std::string>> ipc_buffer_size_attributes_{};
    std::vector<std::string> ipc_buffer_size_group_keys_{};
    std::vector<std::tuple<std::string, std::string>> sql_buffer_size_attributes_{};
    std::vector<std::string> sql_buffer_size_group_keys_{};
    std::vector<std::tuple<std::string, std::string>> table_A1_attributes_{
        std::tuple<std::string, std::string> {"table_name"s, "A"s},
        std::tuple<std::string, std::string> {"index_name"s, "IA1"}
    };
    std::vector<std::string> table_A1_group_keys_{"table_count"s};
    std::vector<std::tuple<std::string, std::string>> table_A2_attributes_{
        std::tuple<std::string, std::string> {"table_name"s, "A"s},
        std::tuple<std::string, std::string> {"index_name"s, "IA2"}
    };
    std::vector<std::string> table_A2_group_keys_{"table_count"s};
    std::vector<std::tuple<std::string, std::string>> table_B_attributes_{
        std::tuple<std::string, std::string> {"table_name"s, "B"s},
        std::tuple<std::string, std::string> {"index_name"s, "IB1"}
    };
    std::vector<std::string> table_B_group_keys_{"table_count"s};

    metrics_metadata const metadata_session_count_{
        "session_count_4_test"s, "number of active sessions"s, session_count_attributes_, session_count_group_keys_
    };
    metrics_metadata const metadata_storage_log_size_{
        "storage_log_size_4_test"s, "transaction log disk usage"s, storage_log_size_attributes_, storage_log_size_group_keys_
    };
    metrics_metadata const metadata_ipc_buffer_size_{
        "ipc_buffer_size_4_test"s, "allocated buffer size for all IPC sessions"s, ipc_buffer_size_attributes_, ipc_buffer_size_group_keys_
    };
    metrics_metadata const metadata_sql_buffer_size_{
        "sql_buffer_size_4_test"s, "allocated buffer size for SQL execution engine"s, sql_buffer_size_attributes_, sql_buffer_size_group_keys_
    };
    metrics_metadata const metadata_table_A1_{
        "index_size_4_test"s, "estimated each index size"s, table_A1_attributes_, table_A1_group_keys_
    };
    metrics_metadata const metadata_table_A2_{
        "index_size_4_test"s, "estimated each index size"s, table_A2_attributes_, table_A2_group_keys_
    };
    metrics_metadata const metadata_table_B_{
        "index_size_4_test"s, "estimated each index size"s, table_B_attributes_, table_B_group_keys_
    };
};

using namespace std::string_view_literals;

TEST_F(metrics_bridge_test, list) {
    auto& metrics_store = bridge_->metrics_store();
    auto& core = bridge_->core();

    auto& session_count = metrics_store.register_item(metadata_session_count_);
    auto& storage_log_size = metrics_store.register_item(metadata_storage_log_size_);
    auto& ipc_buffer_size = metrics_store.register_item(metadata_ipc_buffer_size_);
    auto& sql_buffer_size = metrics_store.register_item(metadata_sql_buffer_size_);
    
    auto& slot_A1 = metrics_store.register_item(metadata_table_A1_);
    auto& slot_A2 = metrics_store.register_item(metadata_table_A2_);
    auto& slot_B = metrics_store.register_item(metadata_table_B_);
    metrics_store.register_aggregation(tateyama::metrics::metrics_aggregation{"table_count", "number of user tables", [](){return std::make_unique<aggregator_for_bridge_test>();}});

    session_count = 100;
    storage_log_size = 65535;
    ipc_buffer_size = 1024;
    sql_buffer_size = 268435456;

    slot_A1 = 65536;
    slot_A2 = 16777216;
    slot_B = 256;

    ::tateyama::proto::metrics::response::MetricsInformation info_list{};
    core.list(info_list);

    int len = info_list.items_size();
//    EXPECT_EQ(len, 6); depends on real implementation
    for (int i = 0; i < len; i++) {
        auto&& item = info_list.items(i);
        auto&& key = item.key();
        auto&& description = item.description();
        if (key == "session_count_4_test") {
            EXPECT_EQ(description, "number of active sessions");
        } else if(key == "storage_log_size_4_test") {
            EXPECT_EQ(description, "transaction log disk usage");
        } else if(key == "ipc_buffer_size_4_test") {
            EXPECT_EQ(description, "allocated buffer size for all IPC sessions");
        } else if(key == "sql_buffer_size_4_test") {
            EXPECT_EQ(description, "allocated buffer size for SQL execution engine");
        } else if(key == "index_size_4_test") {
            EXPECT_EQ(description, "estimated each index size");
        } else if(key == "table_count") {
            EXPECT_EQ(description, "number of user tables");
        }
    }
}

TEST_F(metrics_bridge_test, show) {
    auto& metrics_store = bridge_->metrics_store();
    auto& core = bridge_->core();

    auto& session_count = metrics_store.register_item(metadata_session_count_);
    auto& storage_log_size = metrics_store.register_item(metadata_storage_log_size_);
    auto& ipc_buffer_size = metrics_store.register_item(metadata_ipc_buffer_size_);
    auto& sql_buffer_size = metrics_store.register_item(metadata_sql_buffer_size_);
    
    auto& slot_A1 = metrics_store.register_item(metadata_table_A1_);
    auto& slot_A2 = metrics_store.register_item(metadata_table_A2_);
    auto& slot_B = metrics_store.register_item(metadata_table_B_);
    metrics_store.register_aggregation(tateyama::metrics::metrics_aggregation{"table_count", "number of user tables", [](){return std::make_unique<aggregator_for_bridge_test>();}});

    session_count = 100;
    storage_log_size = 65535;
    ipc_buffer_size = 1024;
    sql_buffer_size = 268435456;

    slot_A1 = 65536;
    slot_A2 = 16777216;
    slot_B = 256;

    ::tateyama::proto::metrics::response::MetricsInformation info_show{};
    core.show(info_show);

    int len = info_show.items_size();
//    EXPECT_EQ(len, 6); depends on real implementation
    for (int i = 0; i < len; i++) {
        auto&& item = info_show.items(i);
        auto&& key = item.key();
        auto&& value = item.value();
        if (key == "session_count_4_test") {
            EXPECT_EQ(value.value_or_array_case(), ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kValue);
            EXPECT_EQ(value.value(), 100);
        } else if(key == "storage_log_size_4_test") {
            EXPECT_EQ(value.value_or_array_case(), ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kValue);
            EXPECT_EQ(value.value(), 65535);
        } else if(key == "ipc_buffer_size_4_test") {
            EXPECT_EQ(value.value_or_array_case(), ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kValue);
            EXPECT_EQ(value.value(), 1024);
        } else if(key == "sql_buffer_size_4_test") {
            EXPECT_EQ(value.value_or_array_case(), ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kValue);
            EXPECT_EQ(value.value(), 268435456);
        } else if(key == "index_size_4_test") {
            EXPECT_EQ(value.value_or_array_case(), ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kArray);
            auto&& array = value.array();
            int alen = array.elements_size();
            for (int j = 0; j < alen; j++) {
                auto&& element = array.elements(j);
                auto&& attributes = element.attributes();
//                if (!attributes.contains("table_name") || !attributes.contains("index_name")) {  // N.G. on ubuntu 20.04
                if ((attributes.find("table_name") == attributes.end()) || (attributes.find("index_name") == attributes.end())) {
                    FAIL();
                }
                auto&& table_name = attributes.at("table_name");
                auto&& index_name = attributes.at("index_name");
                if (table_name == "A" && index_name == "IA1") {
                    EXPECT_EQ(element.value(), 65536);
                } else if (table_name == "A" && index_name == "IA2") {
                    EXPECT_EQ(element.value(), 16777216);
                } else if (table_name == "B" && index_name == "IB1") {
                    EXPECT_EQ(element.value(), 256);
                } else {
                    FAIL();
                }
            }
        } else if(key == "table_count") {
            EXPECT_EQ(value.value_or_array_case(), ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kValue);
            EXPECT_EQ(value.value(), 3);
        }
    }
}

}
