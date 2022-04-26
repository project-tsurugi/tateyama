/*
 * Copyright 2019-2022 tsurugi project.
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

#include <set>
#include <string_view>
#include <mutex>
#include <condition_variable>

#include <tateyama/api/endpoint/response.h>

#include "stream.h"
#include "stream_request.h"

namespace tateyama::common::stream {

class stream_socket;
class stream_request;

template<class T>
struct pointer_comp {
    using is_transparent = std::true_type;
    // helper does some magic in order to reduce the number of
    // pairs of types we need to know how to compare: it turns
    // everything into a pointer, and then uses `std::less<T*>`
    // to do the comparison:
    class helper {
        T* ptr;
    public:
        helper():ptr(nullptr) {}
        helper(helper const&) = default;
        helper(helper&&) noexcept = default;
        helper& operator = (helper const&) = default;
        helper& operator = (helper&&) noexcept = default;
        helper(T* const p):ptr(p) {}  //NOLINT
        template<class U>
        helper( std::shared_ptr<U> const& sp ):ptr(sp.get()) {}  //NOLINT
        template<class U, class...Ts>
        helper( std::unique_ptr<U, Ts...> const& up ):ptr(up.get()) {}  //NOLINT
        ~helper() = default;
        // && optional: enforces rvalue use only
        bool operator<( helper const o ) const {
            return std::less<T*>()( ptr, o.ptr );
        }
    };
    // without helper, we would need 2^n different overloads, where
    // n is the number of types we want to support (so, 8 with
    // raw pointers, unique pointers, and shared pointers).  That
    // seems silly:
    // && helps enforce rvalue use only
    bool operator()( helper const&& lhs, helper const&& rhs ) const {
        return lhs < rhs;
    }
};

class stream_data_channel;

/**
 * @brief writer object for stream_endpoint
 */
class stream_writer : public tateyama::api::endpoint::writer {
    friend stream_data_channel;

public:
    explicit stream_writer(stream_socket& socket, unsigned int slot, unsigned char writer_id)
        : resultset_socket_(socket), slot_(slot), writer_id_(writer_id) {}

    tateyama::status write(char const* data, std::size_t length) override;
    tateyama::status commit() override;

private:
    stream_socket& resultset_socket_;
    unsigned char slot_;
    unsigned char writer_id_;
};

/**
 * @brief data_channel object for stream_endpoint
 */
class stream_data_channel : public tateyama::api::endpoint::data_channel {
public:
    stream_data_channel() = delete;
    explicit stream_data_channel(stream_socket& session_socket, unsigned int slot) : session_socket_(session_socket), slot_(slot) {}
    tateyama::status acquire(tateyama::api::endpoint::writer*& wrt) override;
    tateyama::status release(tateyama::api::endpoint::writer& wrt) override;
    [[nodiscard]] unsigned int get_slot() const { return slot_; }

private:
    stream_socket& session_socket_;
    std::set<std::unique_ptr<stream_writer>, pointer_comp<stream_writer>> data_writers_{};
    unsigned int slot_;
//    std::mutex mutex_{};
//    std::condition_variable condition_{};
    unsigned char writer_id_{};
};

/**
 * @brief response object for stream_endpoint
 */
class stream_response : public tateyama::api::endpoint::response {
public:
    stream_response(stream_request& request, unsigned char index);
    stream_response() = delete;

    void code(tateyama::api::endpoint::response_code code) override;
    tateyama::status body(std::string_view body) override;
    tateyama::status body_head(std::string_view body_head) override;
    tateyama::status acquire_channel(std::string_view name, tateyama::api::endpoint::data_channel*& ch) override;
    tateyama::status release_channel(tateyama::api::endpoint::data_channel& ch) override;
    tateyama::status close_session() override;

private:
    stream_socket& session_socket_;
    unsigned char index_;

    tateyama::api::endpoint::response_code response_code_{tateyama::api::endpoint::response_code::unknown};
    std::string message_{};

    std::unique_ptr<stream_data_channel> data_channel_{};
};

}  // tateyama::common::stream
