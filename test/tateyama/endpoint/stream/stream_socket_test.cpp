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

#include "stream_client.h"
#include "tateyama/endpoint/stream/stream.h"
#include "tateyama/endpoint/stream/stream_response.h"
#include "tateyama/logging_helper.h"
#include <cstring>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <tateyama/logging.h>
#include <unistd.h>
namespace tateyama::endpoint::stream {

class MockConnectionSocket : public connection_socket {
public:
  MockConnectionSocket() : connection_socket(0, 0, 0) {}
};

class MockStreamSocket : public stream_socket {
public:
  MockStreamSocket(int socket, std::string_view info,
                   connection_socket *envelope)
      : stream_socket(socket, info, envelope) {
    LOG(INFO) << "MockStreamSocket constructor: socket=" << socket;
  }
};
class stream_socket_test : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {
    ss.reset();
    envelope_.reset();
  }
  std::unique_ptr<connection_socket> envelope_;
  std::shared_ptr<stream_socket> ss;
};

TEST_F(stream_socket_test, SendTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  std::string data = "abcdefghijklmnopqrstuvwxyz";
  std::uint16_t sport = 1;
  EXPECT_NO_THROW(ss->send(sport, data.c_str(), false));
  EXPECT_NO_THROW(ss->send(sport, data.c_str(), true));
  EXPECT_NO_THROW(ss->send(sport, 'A', "test"));
  EXPECT_NO_THROW(ss->send_result_set_hello(sport, "test"));
  EXPECT_NO_THROW(ss->send_result_set_bye(sport));
  EXPECT_NO_THROW(ss->send_session_bye_ok());
  EXPECT_NO_THROW(ss->change_slot_size(32));
  envelope_->close();
}

TEST_F(stream_socket_test, CloseTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  EXPECT_NO_THROW(ss->close());
  envelope_->close();
}

TEST_F(stream_socket_test, ConnectionInfoTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  EXPECT_EQ(ss->connection_info(),info);
  envelope_->close();
}

#if 0
TEST_F(stream_socket_test, ReleaseSlotTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  //EXPECT_NO_THROW(ss->release_slot(UINT_MAX));
  EXPECT_NO_THROW(ss->release_slot(0));
  envelope_->close();
}

TEST_F(stream_socket_test, LookForSlotTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  LOG(INFO) << "ss->look_for_slot is" << ss->look_for_slot() << socket;
  EXPECT_NO_THROW(ss->look_for_slot());
  envelope_->close();
}
#endif

} // namespace tateyama::endpoint::stream
