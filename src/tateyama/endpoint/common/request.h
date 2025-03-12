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
#pragma once

#include <chrono>
#include <string_view>
#include <filesystem>
#include <sstream>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

#include <tateyama/api/server/request.h>

#include "tateyama/status/resource/database_info_impl.h"
#include "session_info_impl.h"
#include "tateyama/endpoint/common/endpoint_proto_utils.h"
#include "worker_configuration.h"

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
    enum class blob_error : std::int64_t {
        ok = 0,
        not_allowed,
        not_found,
        not_accessible,
        not_regular_file,
    };

    request(tateyama::endpoint::common::resources& resources,
                     std::size_t local_id,
                     const configuration& conf) :
        session_info_(resources.session_info()), session_store_(resources.session_store()), session_variable_set_(resources.session_variable_set()),
        local_id_(local_id), start_at_(std::chrono::system_clock::now()), conf_(conf) {
    }

    [[nodiscard]] std::size_t session_id() const override {
        return session_id_;
    }

    [[nodiscard]] std::size_t service_id() const override {
        return service_id_;
    }

    [[nodiscard]] tateyama::api::server::database_info const& database_info() const noexcept override {
        return conf_.database_info();
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
        if (blob_error_ != blob_error::ok) {
            return false;
        }
        return blobs_.find(std::string(channel_name)) != blobs_.end();
    }

    [[nodiscard]] tateyama::api::server::blob_info const& get_blob(std::string_view name) const override {
        if (blob_error_ != blob_error::ok) {
            throw std::runtime_error(std::string(to_string_view(blob_error_)));
        }
        if(auto itr = blobs_.find(std::string(name)); itr != blobs_.end()) {
            blob_info_ = std::make_unique<blob_info_impl>(name, std::filesystem::path(itr->second.first), itr->second.second);
            return *blob_info_;
        }
        throw std::runtime_error("not found any blob entry with the given name");
    }

    [[nodiscard]] std::chrono::system_clock::time_point start_at() const noexcept {
        return start_at_;
    }

    [[nodiscard]] blob_error get_blob_error() const noexcept {
        return blob_error_;
    }

    [[nodiscard]] std::string blob_error_message() const noexcept {
        std::ostringstream ss{};
        switch (blob_error_) {
            case blob_error::ok: return {""};
            case blob_error::not_allowed: return {"BLOB handling in privileged mode is not allowed on this endpoint"};
            case blob_error::not_found: ss << "tsurugidb failed to receive BLOB file in privileged mode (not found): "; break;
            case blob_error::not_accessible: ss << "tsurugidb failed to receive BLOB file in privileged mode (cannot read): "; break;
            case blob_error::not_regular_file: ss << "tsurugidb failed to receive BLOB file in privileged mode (not regular file): "; break;
        }
        ss << causing_file_;
        return ss.str();
    }

protected:
    const tateyama::api::server::session_info& session_info_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    tateyama::api::server::session_store& session_store_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    tateyama::session::session_variable_set& session_variable_set_;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

    void parse_framework_header(std::string_view message, parse_result& res) {
        if (parse_header(message, res, blobs_)) {
            session_id_ = res.session_id_;
            service_id_ = res.service_id_;

            if (!blobs_.empty()) {
                if (!conf_.allow_blob_privileged_) {
                    blob_error_ = blob_error::not_allowed;
                    return;
                }
                for (auto&& e: blobs_) {
                    std::error_code ec{};
                    std::filesystem::path p(e.second.first);
                    if (std::filesystem::exists(p, ec) && !ec) {
                        if (is_readable(p)) {
                            if (std::filesystem::is_regular_file(p, ec) && !ec) {
                                if (!std::filesystem::is_symlink(p, ec) && !ec) {
                                    continue;
                                }
                            }
                            blob_error_ = blob_error::not_regular_file;
                            causing_file_ = e.second.first;
                            return;
                        }
                        blob_error_ = blob_error::not_accessible;
                        causing_file_ = e.second.first;
                        return;
                    }
                    blob_error_ = blob_error::not_found;
                    causing_file_ = e.second.first;
                    return;
                }
            }
            return;
        }
        throw std::runtime_error("error in parse framework header");
    }
    
private:
    std::size_t local_id_;
    std::chrono::system_clock::time_point start_at_;
    const configuration& conf_;

    std::size_t session_id_{};
    std::size_t service_id_{};
    std::map<std::string, std::pair<std::filesystem::path, bool>> blobs_{};
    blob_error blob_error_{blob_error::ok};
    std::filesystem::path causing_file_{};

    mutable std::unique_ptr<blob_info_impl> blob_info_{};


    [[nodiscard]] constexpr inline std::string_view to_string_view(blob_error value) const noexcept {
        using namespace std::string_view_literals;
        switch (value) {
            case blob_error::ok: return "ok"sv;
            case blob_error::not_allowed: return "not_allowed"sv;
            case blob_error::not_found: return "not_found"sv;
            case blob_error::not_accessible: return "not_accessible"sv;
            case blob_error::not_regular_file: return "not_regular_file"sv;
        }
        std::abort();
    }

    [[nodiscard]] bool is_readable(const std::filesystem::path& p) {
        return access(p.c_str(), R_OK) == 0;
    }

};

}  // tateyama::endpoint::common
