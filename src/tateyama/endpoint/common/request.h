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
#pragma once

#include <chrono>
#include <string_view>
#include <filesystem>

#include <tateyama/api/server/request.h>

#include "tateyama/status/resource/database_info_impl.h"
#include "session_info_impl.h"
#include "tateyama/endpoint/common/endpoint_proto_utils.h"

namespace tateyama::endpoint::common {
/**
 * @brief request object for common_endpoint
 */
class request : public tateyama::api::server::request {
    class blob_info_impl : public tateyama::api::server::blob_info {
    public:
        blob_info_impl(std::string_view channel_name, std::filesystem::path path, bool temporary)
            : channel_name_(channel_name), path_(std::move(path)), temporary_(temporary) {
        }
        
        [[nodiscard]] std::string_view channel_name() const noexcept override {
            return channel_name_;
        }
        [[nodiscard]] std::filesystem::path path() const noexcept override {
            return path_;
        }
        [[nodiscard]] bool is_temporary() const noexcept override {
            return temporary_;
        }
        void dispose() override {
        }
    private:
        const std::string channel_name_{};
        const std::filesystem::path path_{};
        const bool temporary_{};
    };

public:
    explicit request(const tateyama::api::server::database_info& database_info,
                     const tateyama::api::server::session_info& session_info,
                     tateyama::api::server::session_store& session_store,
                     tateyama::session::session_variable_set& session_variable_set,
                     std::size_t local_id) :
        database_info_(database_info), session_info_(session_info), session_store_(session_store), session_variable_set_(session_variable_set),
        local_id_(local_id), start_at_(std::chrono::system_clock::now()) {
    }

    [[nodiscard]] std::size_t session_id() const override {
        return session_id_;
    }

    [[nodiscard]] std::size_t service_id() const override {
        return service_id_;
    }

    [[nodiscard]] tateyama::api::server::database_info const& database_info() const noexcept override {
        return database_info_;
    }

    [[nodiscard]] tateyama::api::server::session_info const& session_info() const noexcept override {
        return session_info_;
    }

    [[nodiscard]] tateyama::api::server::session_store& session_store() noexcept override {
        return session_store_;
    }

    [[nodiscard]] tateyama::session::session_variable_set& session_variable_set() noexcept override {
        return session_variable_set_;
    }

    [[nodiscard]] std::size_t local_id() const noexcept override {
        return local_id_;
    }

    [[nodiscard]] bool has_blob(std::string_view channel_name) const noexcept override {
        return blobs_.find(std::string(channel_name)) != blobs_.end();
    }

    [[nodiscard]] tateyama::api::server::blob_info const& get_blob(std::string_view name) const override {
        if(auto itr = blobs_.find(std::string(name)); itr != blobs_.end()) {
            blob_info_ = std::make_unique<blob_info_impl>(name, std::filesystem::path(itr->second.first), itr->second.second);
            return *blob_info_;
        }
        throw std::runtime_error("blob not found");
    }


    [[nodiscard]] std::chrono::system_clock::time_point start_at() const noexcept {
        return start_at_;
    }

protected:
    const tateyama::api::server::database_info& database_info_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    const tateyama::api::server::session_info& session_info_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    tateyama::api::server::session_store& session_store_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    tateyama::session::session_variable_set& session_variable_set_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    void parse_framework_header(std::string_view message, parse_result& res) {
        if (parse_header(message, res, blobs_)) {
            session_id_ = res.session_id_;
            service_id_ = res.service_id_;
            return;
        }
        throw std::runtime_error("error in parse framework header");
    }
    
private:
    std::size_t session_id_{};
    std::size_t service_id_{};
    std::map<std::string, std::pair<std::filesystem::path, bool>> blobs_{};

    std::size_t local_id_{};
    mutable std::unique_ptr<blob_info_impl> blob_info_{};
    std::chrono::system_clock::time_point start_at_;
};

}  // tateyama::endpoint::common
