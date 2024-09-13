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
#include "stream_receiver.h"
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
#include <thread>
namespace tateyama::endpoint::stream {

class stream_socket_test : public ::testing::Test {
protected:
  static constexpr std::uint8_t RESPONSE_SESSION_PAYLOAD = 1;
  static constexpr std::uint8_t RESPONSE_RESULT_SET_PAYLOAD = 2;
  static constexpr std::uint8_t RESPONSE_RESULT_SET_HELLO = 5;
  static constexpr std::uint8_t RESPONSE_RESULT_SET_BYE = 6;
  static constexpr std::uint8_t RESPONSE_SESSION_BODYHEAD = 7;
  static constexpr std::uint8_t RESPONSE_SESSION_BYE_OK = 8;  // deprecated

  static constexpr int PORT_FOR_TEST = 12350;

  void SetUp() override {
    envelope_ = std::make_unique<connection_socket>(PORT_FOR_TEST);
    receiver_ = std::make_unique<stream_receiver>(PORT_FOR_TEST);
    receiver_thread_ = std::thread(std::ref(*receiver_));
  }

  void TearDown() override {
    envelope_->close();
    ss_.reset();
    envelope_.reset();
    if (receiver_thread_.joinable()) {
      receiver_thread_.join();
    }
  }
  std::unique_ptr<connection_socket> envelope_;
  std::shared_ptr<stream_socket> ss_;
  std::unique_ptr<stream_receiver> receiver_;
  std::thread receiver_thread_;
};

TEST_F(stream_socket_test, Send_and_Close_Test) {
  std::string_view info = "abcdefgh";
  ss_ = envelope_->accept();

  std::string data = "abcdefghijklmnopqrstuvwxyz";
  std::uint16_t slot = 1;
  EXPECT_NO_THROW(ss_->send(slot, data.c_str(), false));
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), slot);
  EXPECT_EQ(receiver_->type(), RESPONSE_SESSION_BODYHEAD);
  EXPECT_EQ(receiver_->message(), data);

  EXPECT_NO_THROW(ss_->send(slot, data.c_str(), true));
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), slot);
  EXPECT_EQ(receiver_->type(), RESPONSE_SESSION_PAYLOAD);
  EXPECT_EQ(receiver_->message(), data);
  
  EXPECT_NO_THROW(ss_->send(slot, 5, "test"));
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), slot);
  EXPECT_EQ(receiver_->writer(), 5);
  EXPECT_EQ(receiver_->message(), "test");
  EXPECT_EQ(receiver_->type(), RESPONSE_RESULT_SET_PAYLOAD);

  EXPECT_NO_THROW(ss_->send_result_set_hello(slot, "test"));
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), slot);
  EXPECT_EQ(receiver_->message(), "test");
  EXPECT_EQ(receiver_->type(), RESPONSE_RESULT_SET_HELLO);
  
  EXPECT_NO_THROW(ss_->send_result_set_bye(slot));
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), slot);
  EXPECT_EQ(receiver_->type(), RESPONSE_RESULT_SET_BYE);

  EXPECT_NO_THROW(ss_->send_session_bye_ok());
  receiver_->wait();
  EXPECT_EQ(receiver_->type(), RESPONSE_SESSION_BYE_OK);
  
  EXPECT_NO_THROW(ss_->change_slot_size(32));
  EXPECT_NO_THROW(ss_->close());
}

TEST_F(stream_socket_test, ConnectionInfoTest) {
  ss_ = envelope_->accept();

  EXPECT_EQ(ss_->connection_info(), receiver_->local_addr());
  EXPECT_NO_THROW(ss_->close());
}

} // namespace tateyama::endpoint::stream
