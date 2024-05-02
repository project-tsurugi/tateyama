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
#include <tateyama/metrics/resource/metrics_store_impl.h>

#include <tateyama/metrics/resource/bridge.h>

#include <gtest/gtest.h>
#include <tateyama/utils/test_utils.h>

namespace tateyama::metrics {

using namespace std::literals::string_literals;
using namespace std::string_view_literals;

class metrics_resource_test :
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
    
    metrics_metadata const metadata_session_count_{
        "session_count"s, "number of active sessions"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {}
    };
    metrics_metadata const metadata_storage_log_size_{
        "storage_log_size"s, "transaction log disk usage"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {}
    };
    metrics_metadata const metadata_ipc_buffer_size_{
        "ipc_buffer_size"s, "allocated buffer size for all IPC sessions"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {}
    };
    metrics_metadata const metadata_sql_buffer_size_{
        "sql_buffer_size"s, "allocated buffer size for SQL execution engine"s,
        std::vector<std::tuple<std::string, std::string>> {},
        std::vector<std::string> {}
    };
};

TEST_F(metrics_resource_test, basic) {
    auto& session_count = metrics_store_->register_item(metadata_session_count_);
    auto& storage_log_size = metrics_store_->register_item(metadata_storage_log_size_);
    auto& ipc_buffer_size = metrics_store_->register_item(metadata_ipc_buffer_size_);
    auto& sql_buffer_size = metrics_store_->register_item(metadata_sql_buffer_size_);

    session_count = 100;
    storage_log_size = 65535;
    ipc_buffer_size = 1024;
    sql_buffer_size = 268435456;

    metrics_store_->enumerate_items([](metrics_metadata const& m, double v) {
        auto key = m.key();
        if (key == "session_count") {
            EXPECT_EQ(100., v);
        } else if(key == "storage_log_size") {
            EXPECT_EQ(65535., v);
        } else if(key == "ipc_buffer_size") {
            EXPECT_EQ(1024., v);
        } else if(key == "sql_buffer_size") {
            EXPECT_EQ(268435456., v);
        } else {
            FAIL();
        }
    });
}

}
