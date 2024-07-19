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
  /*
    MockConnectionSocket(std::uint32_t port, std::size_t timeout,
                         std::size_t socket_limit)
        : connection_socket(port, timeout, socket_limit) {}
        */
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
class stream_write_test : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {
    sw.reset();
    ss.reset();
    envelope_.reset();
  }
  std::unique_ptr<connection_socket> envelope_;
  std::shared_ptr<stream_socket> ss;
  std::shared_ptr<stream_writer> sw;
};

// commit メソッドのテスト
TEST_F(stream_write_test, CommitTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  sw = std::make_shared<stream_writer>(ss, 1, 2);
  EXPECT_EQ(sw->commit(), tateyama::status::ok);
  envelope_->close();
}
TEST_F(stream_write_test, WriteTest) {
  envelope_ = std::make_unique<MockConnectionSocket>();
  std::string_view info = "abcdefgh";
  ss = std::make_unique<MockStreamSocket>(socket(AF_INET, SOCK_STREAM, 0), info,
                                          envelope_.get());
  sw = std::make_shared<stream_writer>(ss, 1, 2);
  std::string data = "abcdefghijklmnopqrstuvwxyz";
  std::size_t length = data.length();
  EXPECT_EQ(sw->write(data.c_str(), length), tateyama::status::ok);
  EXPECT_EQ(sw->write(data.c_str(), INT_MIN), tateyama::status::ok);
  EXPECT_EQ(sw->write(data.c_str(), INT_MAX), tateyama::status::ok);
  EXPECT_EQ(sw->write(nullptr, 0), tateyama::status::ok);
  EXPECT_EQ(sw->write(nullptr, INT_MIN), tateyama::status::ok);
  EXPECT_EQ(sw->write(nullptr, INT_MAX), tateyama::status::ok);
  std::uint16_t sport = 1;
  ss->send(sport, data.c_str(), false);
  EXPECT_NO_THROW(ss->send(sport, data.c_str(), false));
  EXPECT_NO_THROW(ss->send(sport, data.c_str(), true));
  envelope_->close();
}

} // namespace tateyama::endpoint::stream
