/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <sstream>
#include <exception>
#include <stdlib.h>

#include <nlohmann/json.hpp>

#include <tateyama/proto/system/response.pb.h>

namespace tateyama::system::service {

/**
 * @brief tsurugi-info.json handler
 */
class info_handler {
public:
    info_handler() {
        parse_info(info_file_path());
    }

    [[nodiscard]] bool error() const {
        return error_;
    }
    [[nodiscard]] std::string_view error_message() const {
        return error_message_;
    }
    void set_system_info(tateyama::proto::system::response::SystemInfo* system_info) {
        system_info->set_name(name_);
        system_info->set_version(version_);
        system_info->set_date(date_);
        system_info->set_url(url_);
    }
    [[nodiscard]] std::string to_string() {
        std::stringstream ss{};
        ss << "name = " << name_;
        ss << ", version = " << version_;
        ss << ", date = " << date_;
        ss << ", url = " << url_;
        return ss.str();
    }

private:
    std::string name_{};
    std::string version_{};
    std::string date_{};
    std::string url_{};
    std::string error_message_{};
    bool error_{};
    const std::filesystem::path info_path = std::filesystem::path("lib/tsurugi-info.json");

    std::filesystem::path info_file_path() {
        auto env_tsurugi_home = getenv("TSURUGI_HOME");
        if (env_tsurugi_home) {
            std::filesystem::path tsurugi_home(env_tsurugi_home);
            return tsurugi_home / info_path;
        }
        return std::filesystem::canonical("/proc/self/exe").parent_path().parent_path() / info_path;
    }

    void parse_info(const std::filesystem::path& path) {
        using namespace std::literals::string_literals;
        if (std::filesystem::exists(path)) {
            try {
                std::ifstream stream{path};
                nlohmann::json j = nlohmann::json::parse(stream);

                if (j.find("name") != j.end()) {
                    name_ = j["name"];
                }
                if (j.find("version") != j.end()) {
                    version_ = j["version"];
                }
                if (j.find("date") != j.end()) {
                    date_ = j["date"];
                }
                if (j.find("url") != j.end()) {
                    url_ = j["url"];
                }
            } catch (const std::exception &ex) {
                error_ = true;
                error_message_ = "broken json, file = "s + path.string();
            }
        } else {
            error_ = true;
            error_message_ = "cannot find system information file, file name: "s + path.string();
        }
    }
};

}
