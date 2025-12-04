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

#include <filesystem>
#include <exception>
#include <stdlib.h>

#include <nlohmann/json.hpp>

namespace tateyama::system::service {

/**
 * @brief tsurugi-info.json handler
 */
class info_handler {
public:
    info_handler() {
        parse_info(info_file_path());
    }

    std::string get_version() {
        if (!error_) {
            return version_;
        }
        throw std::runtime_error("cannot find version information");
    }

private:
    std::string version_{};
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
        if (std::filesystem::exists(path)) {
            try {
                std::ifstream stream{path};
                nlohmann::json j = nlohmann::json::parse(stream);

                if (j.find("version") != j.end()) {
                    version_ = j["version"];
                }
            } catch (std::exception &ex) {
                error_ = true;
            }
        } else {
            error_ = true;
        }
    }
};

}
