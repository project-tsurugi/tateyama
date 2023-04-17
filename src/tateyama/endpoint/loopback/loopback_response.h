/*
 * Copyright 2019-2023 tsurugi project.
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
#pragma once

#include <map>

#include <tateyama/api/server/response.h>

namespace tateyama::common::loopback {

class loopback_data_writer: public tateyama::api::server::writer {
public:
    explicit loopback_data_writer(std::vector<std::string> &data) :
            data_(data) {
    }

    tateyama::status write(const char *data, std::size_t length) override {
        // NOTE: data is binary data. It maybe data="\0\1\2\3", length=4 etc.
        std::string s { data, length };
        current_data_ += s;
        return tateyama::status::ok;
    }

    tateyama::status commit() override {
        data_.emplace_back(current_data_);
        current_data_.clear();
        return tateyama::status::ok;
    }

private:
    std::string current_data_ { };
    std::vector<std::string> &data_;
};

class loopback_data_channel: public tateyama::api::server::data_channel {
public:
    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer> &wrt) override {
        wrt = std::make_shared < loopback_data_writer > (data_);
        return tateyama::status::ok;
    }

    tateyama::status release(tateyama::api::server::writer&) override {
        return tateyama::status::ok;
    }

    std::vector<std::string>& committed_data() {
        return data_;
    }
private:
    // FIXME: make thread-safe
    std::vector<std::string> data_ { };
};

class loopback_response: public tateyama::api::server::response {
public:
    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }

    void code(tateyama::api::server::response_code code) override {
        code_ = code;
    }

    [[nodiscard]] tateyama::api::server::response_code code() const noexcept {
        return code_;
    }

    tateyama::status body_head(std::string_view body_head) override {
        body_head_ = body_head;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body_head() const noexcept {
        return body_head_;
    }

    tateyama::status body(std::string_view body) override {
        body_ = body;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body() const noexcept {
        return body_;
    }

    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel> &ch)
            override {
        ch = std::make_shared<loopback_data_channel>();
        channel_map_[std::string { name }] = ch;
        return tateyama::status::ok;
    }

    [[nodiscard]] bool has_channel(std::string_view name) const noexcept {
        return channel_map_.find(std::string { name }) != channel_map_.cend();
    }

    [[nodiscard]] std::vector<std::string>& channel(std::string_view name) const {
        // throw out_of_range exception if name doesn't exist in channel_map_
        std::shared_ptr<tateyama::api::server::data_channel> ch = channel_map_.at(std::string { name });
        auto data_channel = dynamic_cast<loopback_data_channel*>(ch.get());
        return data_channel->committed_data();
    }

    tateyama::status release_channel(tateyama::api::server::data_channel&) override {
        return tateyama::status::ok;
    }

    tateyama::status close_session() override {
        return tateyama::status::ok;
    }

private:
    std::size_t session_id_ { };
    tateyama::api::server::response_code code_ { };
    std::string body_head_ { };
    std::string body_ { };

    // FIXME: make thread-safe
    std::map<std::string, std::shared_ptr<tateyama::api::server::data_channel>> channel_map_ { };
};

} // namespace tateyama::common::loopback
