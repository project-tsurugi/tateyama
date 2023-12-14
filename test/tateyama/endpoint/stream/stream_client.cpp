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
#include <arpa/inet.h>  // for inet_addr()
#include <strings.h>    // for bezero()

#include "stream_client.h"

namespace tateyama::api::endpoint::stream {

stream_client::stream_client()
{
    if ((sockfd_ = ::socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        throw std::runtime_error("error in create socket");
    }

    int port = PORT_FOR_TEST;
    struct sockaddr_in client_addr;
    bzero((char *)&client_addr, sizeof(client_addr));
    client_addr.sin_family = PF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = htons(port);

    if (connect(sockfd_, (struct sockaddr *)&client_addr, sizeof(client_addr)) > 0) {
        ::close(sockfd_);
        throw std::runtime_error("connect error");
    }

    send(REQUEST_SESSION_HELLO, 0, "");
    receive();
}

void
stream_client::send(const std::uint8_t type, const std::uint16_t slot, std::string_view message)
{
    std::uint8_t  header[7];  // NOLINT

    std::size_t length = message.length();
    header[0] = type;
    header[1] = slot & 0xff;
    header[2] = (slot >> 8) & 0xff;
    header[3] = length & 0xff;
    header[4] = (length >> 8) & 0xff;
    header[5] = (length >> 16) & 0xff;
    header[6] = (length >> 24) & 0xff;
    ::send(sockfd_, header, 7, 0);
    if (length > 0) {
        ::send(sockfd_, message.data(), length, 0);
    }
}

void stream_client::send(std::string_view message) {
    std::stringstream ss{};
    if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(ss)); ! res) {
        throw std::runtime_error("header serialize error");
    }
    if(auto res = tateyama::utils::PutDelimitedBodyToOstream(message, std::addressof(ss)); ! res) {
        throw std::runtime_error("payload serialize error");
    }
    auto request_message = ss.str();
    send(REQUEST_SESSION_PAYLOAD, 1, request_message);
}

void
stream_client::receive()
{
    std::uint8_t  data[4];  // NOLINT

    recv(sockfd_, &type_, 1, 0);

    recv(sockfd_, data, 2, 0);  
    slot_ = data[0] | (data[1] << 8);

    if (type_ ==  RESPONSE_RESULT_SET_PAYLOAD) {
        ::recv(sockfd_, &writer_, 1, 0);
    }

    ::recv(sockfd_, data, 4, 0);
    std::size_t length = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    if (length > 0) {
        message_.resize(length);
        ::recv(sockfd_, message_.data(), length, 0);
    } else {
        message_.clear();
    }
}

} // namespace tateyama::api::endpoint::stream
