/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include "tateyama/endpoint/ipc/bootstrap/ipc_worker.h"
#include "tateyama/endpoint/header_utils.h"
#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/status/resource/database_info_impl.h"
#include "ipc_client.h"

#include <gtest/gtest.h>

namespace tateyama::server {
class ipc_listener_for_test {
public:
    static void run(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        worker.invoke([&]{
            worker.run();
            worker.delete_hook();
        });
    }
    static void wait(tateyama::endpoint::ipc::bootstrap::ipc_worker& worker) {
        while (!worker.is_terminated());
    }
};
}  // namespace tateyama::server

namespace tateyama::endpoint::ipc {

class blob_info_for_test : public tateyama::api::server::blob_info {
public:
    blob_info_for_test(std::string_view channel_name, std::filesystem::path path, bool temporary)
        : channel_name_(channel_name), path_(std::move(path)), temporary_(temporary) {
    }
    [[nodiscard]] std::string_view channel_name() const noexcept override {
        return channel_name_;
    }
    [[nodiscard]] std::filesystem::path path() const noexcept override {
        return path_;
    }
    [[nodiscard]] bool is_temporary() const noexcept override {
        return temporary_;
    }
    void dispose() override {
    }
private:
    const std::string channel_name_{};
    const std::filesystem::path path_{};
    const bool temporary_{};
};

static constexpr std::size_t my_session_id = 123;

static constexpr std::string_view database_name = "ipc_lob_test";
static constexpr std::string_view label = "label_fot_test";
static constexpr std::string_view application_name = "application_name_fot_test";
static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
static constexpr std::string_view response_test_message = "opqrstuvwxyz";
static constexpr std::string_view request_test_message = "abcdefgh";
static constexpr std::size_t service_id_of_lob_service = 102;

class lob_service : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }
    std::string_view label() const noexcept { return __func__; }

    id_type id() const noexcept { return service_id_of_lob_service; }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        req_ = req;
        for (auto&& e: blobs_) {
            res->add_blob(std::make_unique<blob_info_for_test>(std::get<0>(e), std::get<1>(e), std::get<2>(e)));
        }
        res->body(response_test_message);
        return true;
    }

    tateyama::api::server::request* request() {
        return req_.get();
    }

    void push_blob(const std::string& name, const std::string& path, const bool temporary) {
        blobs_.emplace(name, std::filesystem::path(path), temporary);
    }

private:
    std::shared_ptr<tateyama::api::server::request> req_{};
    std::set<std::tuple<std::string, std::filesystem::path, bool>> blobs_{};
};

class ipc_lob_test : public ::testing::Test {
    virtual void SetUp() {
        auto rv = system("if [ -f /dev/shm/ipc_lob_test ]; then rm -f /dev/shm/ipc_lob_test; fi");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // server part
        std::string session_name{database_name};
        session_name += "-";
        session_name += std::to_string(my_session_id);
        wire_ = std::make_shared<bootstrap::server_wire_container_impl>(session_name, "dummy_mutex_file_name", datachannel_buffer_size, 16);
        const tateyama::endpoint::common::worker_common::configuration conf(tateyama::endpoint::common::worker_common::connection_type::ipc);
        worker_ = std::make_unique<tateyama::endpoint::ipc::bootstrap::ipc_worker>(service_, conf, my_session_id, wire_, database_info_);
        tateyama::server::ipc_listener_for_test::run(*worker_);

        // client part
        tateyama::proto::endpoint::request::ClientInformation cci{};
        cci.set_connection_label(std::string(label));
        cci.set_application_name(std::string(application_name));
        tateyama::proto::endpoint::request::Credential cred{};
        // FIXME handle userName when a credential specification is fixed.
        cci.set_allocated_credential(&cred);
        tateyama::proto::endpoint::request::Handshake hs{};
        hs.set_allocated_client_information(&cci);
        client_ = std::make_unique<ipc_client>(database_name, my_session_id, hs);
        cci.release_credential();
        hs.release_client_information();
    }

    virtual void TearDown() {
        worker_->terminate(tateyama::session::shutdown_request_type::forceful);
        tateyama::server::ipc_listener_for_test::wait(*worker_);

        auto rv = system("if [ -f /dev/shm/ipc_lob_test ]; then rm -f /dev/shm/ipc_lob_test; fi");
        blobs_.clear();
    }

public:
    tateyama::status_info::resource::database_info_impl database_info_{database_name};
    std::set<std::tuple<std::string, std::string, bool>> blobs_{};

protected:
    std::shared_ptr<bootstrap::server_wire_container_impl> wire_{};
    std::unique_ptr<tateyama::endpoint::ipc::bootstrap::ipc_worker> worker_{};
    std::unique_ptr<ipc_client> client_{};
    lob_service service_{};
};


TEST_F(ipc_lob_test, receive) {
    blobs_.emplace("BlobChannel-123-456", "/tmp/BlobFile", false);
    blobs_.emplace("ClobChannel-123-789", "/tmp/ClobFile", true);

    // we do not care service_id nor request message here
    client_->send(service_id_of_lob_service, std::string(request_test_message), blobs_);
    std::string res{};
    client_->receive(res);

    // server part
    auto* request = service_.request();

    // test for blob_info
    // blob
    EXPECT_TRUE(request->has_blob("BlobChannel-123-456"));
    auto& blob = request->get_blob("BlobChannel-123-456");
    EXPECT_EQ(blob.channel_name(), "BlobChannel-123-456");
    EXPECT_EQ(blob.path().string(), "/tmp/BlobFile");
    EXPECT_EQ(blob.is_temporary(), false);

    // clob
    EXPECT_TRUE(request->has_blob("ClobChannel-123-789"));
    auto& clob = request->get_blob("ClobChannel-123-789");
    EXPECT_EQ(clob.channel_name(), "ClobChannel-123-789");
    EXPECT_EQ(clob.path().string(), "/tmp/ClobFile");
    EXPECT_EQ(clob.is_temporary(),true);

    // blob, clob that does not exist
    EXPECT_FALSE(request->has_blob("BlobChannel-987-654"));
    EXPECT_THROW(auto& blob_not_find = request->get_blob("BlobChannel-987-654"), std::runtime_error);
    EXPECT_FALSE(request->has_blob("ClobChannel-654-321"));
    EXPECT_THROW(auto& clob_not_find = request->get_blob("ClobChannel-654-321"), std::runtime_error);
}

TEST_F(ipc_lob_test, send) {
    service_.push_blob("BlobChannel-123-456", "/tmp/BlobFile", false);
    service_.push_blob("ClobChannel-123-789", "/tmp/ClobFile", true);

    // we do not care service_id nor request message here
    client_->send(service_id_of_lob_service, std::string(request_test_message), blobs_);
    std::string res{};
    client_->receive(res);

    ::tateyama::proto::framework::response::Header& header = client_->framework_response_header();
    ASSERT_TRUE(header.has_blobs());
    for (auto&& e: header.blobs().blobs()) {
        if (e.channel_name().compare("BlobChannel-123-456") == 0) {
            EXPECT_EQ(e.channel_name(), "BlobChannel-123-456");
            EXPECT_EQ(e.path(), "/tmp/BlobFile");
            EXPECT_EQ(e.temporary(), false);
        } else if (e.channel_name().compare("ClobChannel-123-789") == 0) {
            EXPECT_EQ(e.channel_name(), "ClobChannel-123-789");
            EXPECT_EQ(e.path(), "/tmp/ClobFile");
            EXPECT_EQ(e.temporary(), true);
        }
    }
}

} // namespace tateyama::endpoint::ipc
