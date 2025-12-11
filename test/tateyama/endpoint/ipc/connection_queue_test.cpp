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
#include "ipc_gtest_base.h"

#include <vector>
#include <cstdint>
#include <thread>

#include "tateyama/endpoint/ipc/bootstrap/server_wires_impl.h"

namespace tateyama::endpoint::ipc {

static constexpr std::string_view database_name = "connection_queue_test";
static constexpr std::size_t threads = 104;
static constexpr std::uint8_t admin_sessions = 1;

class connection_queue_test : public ::testing::Test {
    class listener {
        static constexpr std::size_t inactive_session_id = UINT64_MAX;

    public:
        explicit listener(tateyama::endpoint::ipc::bootstrap::connection_container& container) : container_(container) {
            sessions_.resize(threads + admin_sessions);
            for (auto& s : sessions_) {
                s = inactive_session_id;
            }
        }

        void operator()() {
            auto& connection_queue = container_.get_connection_queue();

            while(true) {
                std::size_t session_id = connection_queue.listen();
                if (connection_queue.is_terminated()) {
                    break;
                }
                std::size_t index = connection_queue.slot();
                auto idx = tateyama::common::wire::connection_queue::reset_admin(index);
                if (reject_) {
                    connection_queue.reject(index);
                } else {
                    EXPECT_EQ(sessions_.at(idx), inactive_session_id);
                    sessions_.at(idx) = session_id;
                    connection_queue.accept(index, session_id);
                }
            }
            connection_queue.confirm_terminated();
        }

        void disconnect(std::size_t index, std::size_t session_id) {
            auto idx = tateyama::common::wire::connection_queue::reset_admin(index);
            EXPECT_EQ(sessions_.at(idx), session_id);
            sessions_.at(idx) = inactive_session_id;
            container_.get_connection_queue().disconnect(index);
        }

        void set_reject_mode() {
            reject_ = true;
        }

    private:
        tateyama::endpoint::ipc::bootstrap::connection_container& container_;
        std::vector<std::size_t> sessions_{};
        bool reject_{};
    };

    void SetUp() override {
        rv_ = ::system("if [ -f /dev/shm/connection_queue_test ]; then rm -f /dev/shm/connection_queue_test; fi");
        container_ = std::make_unique < tateyama::endpoint::ipc::bootstrap::connection_container > (database_name, threads, admin_sessions);
        listener_ = std::make_unique < listener > (*container_);
        listener_thread_ = std::thread(std::ref(*listener_));
    }

    void TearDown() override {
        container_->get_connection_queue().request_terminate();
        listener_thread_.join();
        rv_ = ::system("if [ -f /dev/shm/connection_queue_test ]; then rm -f /dev/shm/connection_queue_test; fi");
    }

    int rv_;

protected:
    std::size_t connect(std::size_t& slot) {
        slot = container_->get_connection_queue().request();
        return container_->get_connection_queue().wait(slot);
    }

    std::size_t connect_admin(std::size_t& slot) {
        slot = container_->get_connection_queue().request_admin();
        return container_->get_connection_queue().wait(slot);
    }

    std::size_t connect() {
        std::size_t slot{};
        return connect(slot);
    }

    std::size_t connect_admin() {
        std::size_t slot{};
        return connect_admin(slot);
    }


    void disconnect(std::size_t index, std::size_t session_id) {
        listener_->disconnect(index, session_id);
    }

    void set_reject_mode() {
        listener_->set_reject_mode();
    }

private:
    std::unique_ptr<tateyama::endpoint::ipc::bootstrap::connection_container> container_ { };
    std::unique_ptr<listener> listener_{};
    std::thread listener_thread_;
};

TEST_F(connection_queue_test, normal_session_limit) {
    std::vector<std::size_t> session_ids{};

    for (std::size_t i = 0; i < threads; i++) {
        session_ids.emplace_back(connect());
    }

    EXPECT_THROW(connect(), std::runtime_error);
}

TEST_F(connection_queue_test, admin_session) {
    std::vector<std::size_t> session_ids{};

    for (std::size_t i = 0; i < threads; i++) {
        session_ids.emplace_back(connect());
    }

    session_ids.emplace_back(connect_admin());

    EXPECT_THROW(connect(), std::runtime_error);
    EXPECT_THROW(connect_admin(), std::runtime_error);
}

TEST_F(connection_queue_test, reject) {
    set_reject_mode();

    EXPECT_EQ(connect(), UINT64_MAX);
    EXPECT_EQ(connect_admin(), UINT64_MAX);
}

TEST_F(connection_queue_test, many) {
    std::vector<std::thread> pthreads{};

    static constexpr int loop = 512;
    for (std::size_t i = 0; i < threads; i++) {
        pthreads.emplace_back(
            std::thread([this](int n){
                for (int i = 0; i < n; i++) {
                    try {
                        std::size_t slot{};
                        auto sid = connect(slot);
                        disconnect(slot, sid);
                    } catch (std::runtime_error &ex) {
                        std::cout << ex.what() << std::endl;
                        FAIL();
                    }
                }
            }, loop)
        );
    }
    pthreads.emplace_back(
        std::thread([this](int n){
            for (int i = 0; i < n; i++) {
                try {
                    std::size_t slot{};
                    auto sid = connect_admin(slot);
                    disconnect(slot, sid);
                } catch (std::runtime_error &ex) {
                    FAIL();
                }
            }
        }, loop)
    );

    for (auto& thread: pthreads) {
        thread.join();
    }
}

}
