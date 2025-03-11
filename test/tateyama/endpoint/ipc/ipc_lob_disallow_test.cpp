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
class ipc_listener_for_disallow_test {
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

class blob_info_for_disallow_test : public tateyama::api::server::blob_info {
public:
    blob_info_for_disallow_test(std::string_view channel_name, std::filesystem::path path, bool temporary)
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

static constexpr std::string_view database_name = "ipc_lob_disallow_test";
static constexpr std::string_view label = "label_fot_disallow_test";
static constexpr std::string_view application_name = "application_name_fot_disallow_test";
static constexpr std::size_t datachannel_buffer_size = 64 * 1024;
static constexpr tateyama::common::wire::message_header::index_type index_ = 1;
static constexpr std::string_view response_disallow_test_message = "opqrstuvwxyz";
static constexpr std::string_view request_disallow_test_message = "abcdefgh";
static constexpr std::size_t service_id_of_lob_disallow_service = 104;

class lob_disallow_service : public tateyama::framework::routing_service {
public:
    bool setup(tateyama::framework::environment&) { return true; }
    bool start(tateyama::framework::environment&) { return true; }
    bool shutdown(tateyama::framework::environment&) { return true; }
    std::string_view label() const noexcept { return __func__; }

    id_type id() const noexcept { return service_id_of_lob_disallow_service; }
    bool operator ()(std::shared_ptr<tateyama::api::server::request> req,
                     std::shared_ptr<tateyama::api::server::response> res) override {
        visit_++;
        req_ = req;
        for (auto&& e: blobs_) {
            EXPECT_EQ(res->add_blob(std::make_unique<blob_info_for_disallow_test>(std::get<0>(e), std::get<1>(e), std::get<2>(e))), tateyama::status::operation_denied);
        }
        res->body(response_disallow_test_message);
        return true;
    }

    tateyama::api::server::request* request() {
        return req_.get();
    }
    std::size_t visit() {
        return visit_;
    }

    void push_blob(const std::string& name, const std::string& path, const bool temporary) {
        blobs_.emplace(name, std::filesystem::path(path), temporary);
    }

private:
    std::size_t visit_{};
    std::shared_ptr<tateyama::api::server::request> req_{};
    std::set<std::tuple<std::string, std::filesystem::path, bool>> blobs_{};
};

class ipc_lob_disallow_test : public ::testing::Test {
    virtual void SetUp() {
        auto rv = system("if [ -f /dev/shm/ipc_lob_disallow_test ]; then rm -f /dev/shm/ipc_lob_disallow_test; fi");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // server part
        std::string session_name{database_name};
        session_name += "-";
        session_name += std::to_string(my_session_id);
        wire_ = std::make_shared<bootstrap::server_wire_container_impl>(session_name, "dummy_mutex_file_name", datachannel_buffer_size, 16);
        conf_.allow_blob_privileged(false);
        worker_ = std::make_unique<tateyama::endpoint::ipc::bootstrap::ipc_worker>(service_, conf_, my_session_id, wire_);
        tateyama::server::ipc_listener_for_disallow_test::run(*worker_);

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
        tateyama::server::ipc_listener_for_disallow_test::wait(*worker_);

        auto rv = system("if [ -f /dev/shm/ipc_lob_disallow_test ]; then rm -f /dev/shm/ipc_lob_disallow_test; fi");
    }

    tateyama::endpoint::common::configuration conf_{tateyama::endpoint::common::connection_type::ipc, database_info_};

public:
    tateyama::status_info::resource::database_info_impl database_info_{database_name};

protected:
    std::shared_ptr<bootstrap::server_wire_container_impl> wire_{};
    std::unique_ptr<tateyama::endpoint::ipc::bootstrap::ipc_worker> worker_{};
    std::unique_ptr<ipc_client> client_{};
    lob_disallow_service service_{};

    std::filesystem::path test_file(std::string name) {
        std::filesystem::path path = std::filesystem::temp_directory_path();
        path /= name + "-" + std::to_string(getpid());
        return path;
    }
    void create_test_file(std::string name) {
        if (auto fd = open(test_file(name).c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); fd < 0) { // NOLINT
            FAIL();
        }
    }
    void remove_test_file(std::string name) {
        if (unlink(test_file(name).c_str()) != 0) {
            FAIL();
        }
    }
};

TEST_F(ipc_lob_disallow_test, receive) {
    create_test_file("BlobRecFile");
    create_test_file("ClobRecFile");

    std::set<std::tuple<std::string, std::string, bool>> blobs{};
    blobs.emplace("BlobChannel-123-456", test_file("BlobRecFile"), false);
    blobs.emplace("ClobChannel-123-789", test_file("ClobRecFile"), true);

    // we do not care service_id nor request message here
    client_->send(service_id_of_lob_disallow_service, std::string(request_disallow_test_message), blobs);
    tateyama::proto::framework::response::Header::PayloadType type{};
    std::string res{};
    client_->receive(res, type);

    // client check
    EXPECT_EQ(type, tateyama::proto::framework::response::Header::SERVER_DIAGNOSTICS);

    // server check
    EXPECT_EQ(service_.visit(), 0);

    remove_test_file("BlobRecFile");
    remove_test_file("ClobRecFile");
}

TEST_F(ipc_lob_disallow_test, send) {
    service_.push_blob("BlobChannel-123-456", test_file("BlobSndFile"), false);
    service_.push_blob("ClobChannel-123-789", test_file("ClobSndFile"), true);

    // we do not care service_id nor request message here
    client_->send(service_id_of_lob_disallow_service, std::string(request_disallow_test_message));
    std::string res{};
    client_->receive(res);

    ::tateyama::proto::framework::response::Header& header = client_->framework_response_header();
    EXPECT_FALSE(header.has_blobs());
}

} // namespace tateyama::endpoint::ipc
