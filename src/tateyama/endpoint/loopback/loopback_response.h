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

/**
 * @brief thread-safe list of std::string
 */
class concurrent_string_list {
public:
    /**
     * @brief add a string
     * @param data the data to be added
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention calling this function after {@ref get_all()} will change the entries of the return value of it.
     */
    void add(std::string &data) {
        // FIXME: make thread-safe
        list_.emplace_back(data);
    }

    /**
     * @brief get a string list which has been added by {@ref #add(std::string&)}
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention entries will be changed if {@ref #add(std::string&)} is called after this function returns.
     * @attention do not change entries of the return value, or unexpected situation occurs.
     */
    std::vector<std::string>& get_all() {
        return list_;
    }
private:
    // FIXME: make thread-safe
    std::vector<std::string> list_ { };
};

/**
 * @brief writer object for loopback_endpoint
 */
class loopback_data_writer: public tateyama::api::server::writer {
public:
    /**
     * @brief create a empty loopback_data_writer object
     * @param list string list which committed data to be added
     */
    explicit loopback_data_writer(concurrent_string_list &list) :
            list_(list) {
    }

    /**
     * @brief write data
     * @details write out the given data to the application output.
     * This data is opaque binary sequence in this API layer. Typically its format are shared by the endpoint users
     * by having common encoders/decoders.
     * @param data the pointer to the data to be written
     * @param length the byte length of the data to be written
     * @return status::ok when successful
     * @return other status code when error occurs
     * @attention this function is not thread-safe and should be called from single thread at a time.
     * @attention do not call this function and {@ref #commit()} of same writer simultaneously.
     */
    tateyama::status write(const char *data, std::size_t length) override {
        // NOTE: data is binary data. It maybe data="\0\1\2\3", length=4 etc.
        std::string s { data, length };
        current_data_ += s;
        written_ = true;
        return tateyama::status::ok;
    }

    /**
     * @see `tateyama::api::server::writer::commit()`
     * @attention this function is not thread-safe and should be called from single thread at a time.
     * @attention do not call this function and {@ref #write(const char*,std::size_t)} of same writer simultaneously.
     */
    tateyama::status commit() override {
        if (written_) {
            list_.add(current_data_);
            current_data_.clear();
            written_ = false;
        }
        return tateyama::status::ok;
    }

private:
    std::string current_data_ { };
    bool written_ { false };
    concurrent_string_list &list_;
};

/**
 * @brief data_channel object for loopback_endpoint
 */
class loopback_data_channel: public tateyama::api::server::data_channel {
public:
    /**
     * @see `tateyama::api::server::data_channel::acquire()`
     */
    tateyama::status acquire(std::shared_ptr<tateyama::api::server::writer> &wrt) override {
        wrt = std::make_shared < loopback_data_writer > (list_);
        return tateyama::status::ok;
    }

    /**
     * @see `tateyama::api::server::data_channel::release()`
     */
    tateyama::status release(tateyama::api::server::writer&) override {
        // FIXME: check whether the argument is a valid writer or not (unnecessary?).
        // - writer is a instance of loopback_data_channel
        // - acquire()ed by this channel
        // - not release()ed already
        return tateyama::status::ok;
    }

    /**
     * @brief returns committed data to this channel.
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @note do not call this function during writing/committing to this channel,
     * or unexpected value will return.
     */
    std::vector<std::string>& committed_data() {
        return list_.get_all();
    }
private:
    concurrent_string_list list_ { };
};

/**
 * @brief response object for loopback_endpoint
 * @note This object can be used both server and client side.
 * You should use non-const functions at server side to set each values.
 * At client side, use const functions to get each values.
 */
class loopback_response: public tateyama::api::server::response {
public:
    /**
     * @brief clear all data hold in this response object
     * @details To reuse response object, call this function after current response data is unnecessary.
     * @attention this function is not thread-safe and should be called from single thread at a time.
     */
    void clear() {
        session_id_ = 0;
        code_ = tateyama::api::server::response_code::unknown;
        body_head_.clear();
        body_.clear();
        channel_map_.clear();
    }

    /**
     * @see `tateyama::server::response::session_id()`
     */
    void session_id(std::size_t id) override {
        session_id_ = id;
    }

    /**
     * @brief accessor to the session identifier
     * returns the value has set by {@link #session_id(std::size_t)}
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention do not call this function and {@ref #session_id(std::size_t)} simultaneously.
     */
    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }

    /**
     * @see `tateyama::server::response::code()`
     */
    void code(tateyama::api::server::response_code code) override {
        code_ = code;
    }

    /**
     * @brief accessor to the tateyama response status
     * returns the value has set by {@link #code(tateyama::api::server::response_code)}
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention do not call this function and [@link #code(tateyama::api::server::response_code)} simultaneously.
     */
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

    /**
     * @brief accessor to the response body head
     * returns the value has set by {@link #body_head(std::string_view)}
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention do not call this function and [@link #body_head(std::string_view)} simultaneously.
     */
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

    /**
     * @brief accessor to the response body
     * returns the value has set by {@link #body(std::string_view)}
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @attention do not call this function and [@link #body(std::string_view)} simultaneously.
     */
    [[nodiscard]] std::string_view body() const noexcept {
        return body_;
    }

    /**
     * @see `tateyama::server::response::acquire_channel()`
     * @attention This function fails if {@code name} has been already acquired and not released yet.
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
     * @brief returns true if this response has a channel of specified name
     * @param name a name of the channel
     * @return true if this response has a channel of specified name
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     */
    [[nodiscard]] bool has_channel(std::string_view name) const noexcept {
        // FIXME: make thread-safe
        return channel_map_.find(std::string { name }) != channel_map_.cend();
    }

    /**
     * @brief returns a {@code std::vector} of written data to the channel of the specified name
     * @returns written data to the channel of the specified name
     * @throw out_of_range if this response doesn't have the channel of the specified name
     * @note this function is thread-safe and multiple threads can invoke simultaneously.
     * @note do not call this function during writing/committing to the channel of the specified name,
     * or unexpected value will return.
     */
    [[nodiscard]] std::vector<std::string>& channel(std::string_view name) const {
        // FIXME: make thread-safe
        std::shared_ptr<tateyama::api::server::data_channel> ch = channel_map_.at(std::string { name });
        auto data_channel = dynamic_cast<loopback_data_channel*>(ch.get());
        return data_channel->committed_data();
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

private:
    std::size_t session_id_ { };
    tateyama::api::server::response_code code_ { };
    std::string body_head_ { };
    std::string body_ { };

    // FIXME: make thread-safe
    std::map<std::string, std::shared_ptr<tateyama::api::server::data_channel>> channel_map_ { };
};

} // namespace tateyama::common::loopback
