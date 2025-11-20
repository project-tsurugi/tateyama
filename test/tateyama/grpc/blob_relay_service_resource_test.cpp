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

#include <cstdio>
#include <memory>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <tateyama/framework/server.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/grpc/blob_relay_service_resource.h>

#include <gtest/gtest.h>
#include <tateyama/test_utils/utility.h>

namespace tateyama::grpc {

class blob_relay_service_resource_test :
    public ::testing::Test,
    public test_utils::utility
{
public:
    void SetUp() override {
        temporary_.prepare();

        std::stringstream ss{};
        session_store_config(ss);
        auto cfg = std::make_shared<api::configuration::whole>(ss, test_utils::default_configuration_for_tests);
        sv_ = std::make_unique<framework::server>(framework::boot_mode::database_server, cfg);
        add_core_components(*sv_);

        sv_->setup();
        // obtaining the blob_relay_service_resource can be done after setup();
        service_resource_ = std::dynamic_pointer_cast<grpc::blob_relay_service_resource>(sv_->server::find_resource_by_id(framework::resource_id_blob_relay_service));
        if (!service_resource_) {
            FAIL();
        }

        sv_->start();
        // obtaining the blob_relay_service must be done after start().
        try {
            blob_relay_service_ = service_resource_->blob_relay_service();
            if (!blob_relay_service_) {
                FAIL();
            }
        } catch (std::runtime_error &ex) {
            std::cout << ex.what() << std::endl;
            FAIL();
        }
    }
    void TearDown() override {
        sv_->shutdown();
        temporary_.clean();
    }

protected:
    std::shared_ptr<grpc::blob_relay_service_resource> service_resource_{};
    std::shared_ptr<data_relay_grpc::blob_relay::blob_relay_service> blob_relay_service_{};

    std::filesystem::path create_blob_file(std::filesystem::path file) {
        auto file_path = path() / file;
        std::ofstream strm(file_path);
        if (strm) {
            strm << ref_;
            strm.close();
            return file_path;
        }
        return {};
    }

private:
    std::unique_ptr<framework::server> sv_{};
    std::string ref_{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n"};

    void session_store_config(std::stringstream& ss) {
        std::filesystem::path session_store{path() / std::filesystem::path("session_store")};
        if (!std::filesystem::create_directory(session_store)) {
            throw std::runtime_error("cannot create directory for session store");
        }
        ss << "[blob_relay]\n";
        ss << "session_store=" << session_store.string() << '\n';
        ss << "[datastore]\n";
        ss << "log_location=" << path() << '\n';
    }
};

TEST_F(blob_relay_service_resource_test, basic) {
    auto& blob_session = blob_relay_service_->create_session();
    blob_session.dispose();
}

TEST_F(blob_relay_service_resource_test, add_blob) {
    auto& blob_session = blob_relay_service_->create_session();

    auto blob_file_path = create_blob_file("add_blob");
    EXPECT_TRUE(std::filesystem::exists(blob_file_path));
    auto bid = blob_session.add(blob_file_path);
    if (auto path_opt = blob_session.find(bid); bid) {
        EXPECT_EQ(blob_file_path, path_opt.value());
    } else {
        FAIL();
    }
    
    blob_session.dispose();
    EXPECT_FALSE(std::filesystem::exists(blob_file_path));
}

TEST_F(blob_relay_service_resource_test, add_blob_no_file) {
    auto& blob_session = blob_relay_service_->create_session();

    auto blob_file_path = create_blob_file("add_blob_no_file");
    std::filesystem::remove(blob_file_path);

    EXPECT_THROW({ auto bid = blob_session.add(blob_file_path); }, std::runtime_error);
    
    blob_session.dispose();
}

TEST_F(blob_relay_service_resource_test, entries_and_remove) {
    auto& blob_session = blob_relay_service_->create_session();

    auto blob_file_path = create_blob_file("entries_and_remove");
    EXPECT_TRUE(std::filesystem::exists(blob_file_path));
    auto bid = blob_session.add(blob_file_path);

    auto blobs = blob_session.entries();
    EXPECT_EQ(1, blobs.size());
    EXPECT_EQ(blobs.at(0), bid);
    
    blob_session.remove(blobs.begin(), blobs.end());
    EXPECT_FALSE(std::filesystem::exists(blob_file_path));

    blob_session.dispose();
}

} // namespace tateyama::endpoint::ipc
