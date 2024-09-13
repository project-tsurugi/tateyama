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

class stream_write_test : public ::testing::Test {
protected:
  static constexpr int PORT_FOR_TEST = 12351;

  void SetUp() override {
    envelope_ = std::make_unique<connection_socket>(PORT_FOR_TEST);
    receiver_ = std::make_unique<stream_receiver>(PORT_FOR_TEST);
    receiver_thread_ = std::thread(std::ref(*receiver_));
  }

  void TearDown() override {
    envelope_->close();
    sw_.reset();
    ss_.reset();
    envelope_.reset();
    if (receiver_thread_.joinable()) {
      receiver_thread_.join();
    }
  }
  std::unique_ptr<connection_socket> envelope_;
  std::shared_ptr<stream_socket> ss_;
  std::shared_ptr<stream_writer> sw_;
  std::unique_ptr<stream_receiver> receiver_;
  std::thread receiver_thread_;
};

TEST_F(stream_write_test, CommitTest) {
  std::string_view info = "abcdefgh";
  ss_ = envelope_->accept();
  // slot = 1, writer = 2
  sw_ = std::make_shared<stream_writer>(ss_, 1, 2);

  EXPECT_EQ(sw_->commit(), tateyama::status::ok);
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), 1);  
  EXPECT_EQ(receiver_->writer(), 2);  
  EXPECT_EQ(receiver_->message(), "");  

  ss_->close();
}
TEST_F(stream_write_test, WriteTest) {
  std::string_view info = "abcdefgh";
  ss_ = envelope_->accept();
  // slot = 1, writer = 2
  sw_ = std::make_shared<stream_writer>(ss_, 1, 2);

  std::string data = "abcdefghijklmnopqrstuvwxyz";
  std::size_t length = data.length();
  EXPECT_EQ(sw_->write(data.c_str(), length), tateyama::status::ok);

  // do not invoke send()
  EXPECT_EQ(sw_->write(data.c_str(), 0), tateyama::status::ok);

  // do not invoke send()
  EXPECT_EQ(sw_->write(nullptr, 0), tateyama::status::ok);
  EXPECT_EQ(sw_->write(nullptr, INT_MIN), tateyama::status::ok);
  EXPECT_EQ(sw_->write(nullptr, INT_MAX), tateyama::status::ok);

  // send data given by the first write() only
  EXPECT_EQ(sw_->commit(), tateyama::status::ok);
  receiver_->wait();
  EXPECT_EQ(receiver_->slot(), 1);  
  EXPECT_EQ(receiver_->writer(), 2);  
  EXPECT_EQ(receiver_->message(), "abcdefghijklmnopqrstuvwxyz");  

  std::uint16_t slot = 1;
  ss_->send(slot, data.c_str(), false);
  EXPECT_NO_THROW(ss_->send(slot, data.c_str(), false));
  EXPECT_NO_THROW(ss_->send(slot, data.c_str(), true));
  ss_->close();
}

} // namespace tateyama::endpoint::stream
