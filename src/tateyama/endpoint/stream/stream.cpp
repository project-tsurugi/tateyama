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
#include <iostream>

#include "stream.h"
#include "stream_response.h"

namespace tateyama::endpoint::stream {

stream_socket::stream_socket(int socket, std::string_view info, connection_socket* envelope)
    : socket_(socket), connection_info_(info), envelope_(envelope) {
    const int enable = 1;
    const struct timeval timeout{SEND_TIMEOUT, 0};

    if (setsockopt(socket, SOL_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
        LOG_LP(ERROR) << "setsockopt() fail";
    }
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_LP(ERROR) << "setsockopt() fail";
    }
    responses_.resize(slot_size_);
    envelope_->num_open_.fetch_add(1);
}

stream_socket::~stream_socket() {
    close();
    envelope_->num_open_.fetch_sub(1);
    envelope_->notify_of_close();
}

void stream_socket::close_data_channel_by_client(unsigned int slot) {
    auto sp = responses_.at(slot)->data_channel();
    if (sp) {
        sp->set_closed();
    }
}

};
