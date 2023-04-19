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
    tateyama::status write(const char *data, std::size_t length) override {
        if (length > 0) {
            // NOTE: data is binary data. It maybe data="\0\1\2\3", length=4 etc.
            std::string s { data, length };
            current_data_ += s;
        }
        return tateyama::status::ok;
    }

    tateyama::status commit() override {
        if (current_data_.length() > 0) {
            list_.emplace_back(current_data_);
            current_data_.clear();
        }
        return tateyama::status::ok;
    }

    std::vector<std::string> committed_data() {
        return std::move(list_);
    }

private:
    std::string current_data_ { };
    std::vector<std::string> list_ { };
};

class loopback_data_channel: public tateyama::api::server::data_channel {
public:
    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer> &wrt) override {
        // FIXME make thread-safe
        auto writer = std::make_shared<loopback_data_writer>();
        writers_.emplace_back(writer);
        wrt = writer;
        return tateyama::status::ok;
    }

    tateyama::status release(tateyama::api::server::writer&) override {
        // FIXME make thread-safe
        return tateyama::status::ok;
    }

    std::vector<std::string> committed_data() {
        std::vector < std::string > whole { };
        // FIXME make thread-safe
        for (auto &writer : writers_) {
            for (auto &data : writer->committed_data()) {
                whole.emplace_back(data);
            }
        }
        return whole;
    }
private:
    std::vector<std::shared_ptr<loopback_data_writer>> writers_ { };
};

class loopback_response: public tateyama::api::server::response {
public:
    /**
     * @see `tateyama::server::response::session_id()`
     */
    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }

    /**
     * @see `tateyama::server::response::code()`
     */
    void code(tateyama::api::server::response_code code) override {
        code_ = code;
    }

    [[nodiscard]] tateyama::api::server::response_code code() const noexcept {
        return code_;
    }

    /**
     * @see `tateyama::server::response::body_head()`
     */
    tateyama::status body_head(std::string_view body_head) override {
        body_head_ = body_head;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body_head() const noexcept {
        return body_head_;
    }

    /**
     * @see `tateyama::server::response::body()`
     */
    tateyama::status body(std::string_view body) override {
        body_ = body;
        return tateyama::status::ok;
    }

    [[nodiscard]] std::string_view body() const noexcept {
        return body_;
    }

    [[nodiscard]] bool has_channel(std::string_view name) const noexcept {
        // FIXME: make thread-safe
        return channel_map_.find(std::string { name }) != channel_map_.cend();
    }

    /**
     * @see `tateyama::server::response::acquire_channel()`
     * @attention This function fails if {@code name} has been already acquired (even if it has been released).
     */
    tateyama::status acquire_channel(std::string_view name, std::shared_ptr<tateyama::api::server::data_channel> &ch)
            override {
        // FIXME: make thread-safe
        if (has_channel(name)) {
            return tateyama::status::not_found;
        }
        ch = std::make_shared<loopback_data_channel>();
        channel_map_[std::string { name }] = ch;
        return tateyama::status::ok;
    }

    /**
     * @see `tateyama::server::response::release_channel()`
     */
    tateyama::status release_channel(tateyama::api::server::data_channel&) override {
        // FIXME: make thread-safe
        return tateyama::status::ok;
    }

    /**
     * @see `tateyama::server::response::close_session()`
     */
    tateyama::status close_session() override {
        return tateyama::status::ok;
    }

    std::map<std::string, std::vector<std::string>> all_committed_data() {
        std::map<std::string, std::vector<std::string>> data_map { };
        for (const auto& [name, channel] : channel_map_) {
            auto data_channel = dynamic_cast<loopback_data_channel*>(channel.get());
            data_map[name] = data_channel->committed_data();
        }
        return data_map;
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
