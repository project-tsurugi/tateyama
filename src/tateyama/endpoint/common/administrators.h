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

#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace tateyama::endpoint::common {
/**
 * @brief An object to store the administrator's user name
 */
class administrators {
  public:
    explicit administrators(const std::string& names) {
        if (names.empty()) {
            throw std::runtime_error("authentication.administrators is empty");
        }
        if (names == "*") {
            no_authentication_ = true;
            return;
        }
        std::vector<std::string> each_name;
        boost::algorithm::split(each_name, names, boost::is_any_of(","));
        for (auto& e: each_name) {
            boost::algorithm::trim(e);
            names_.emplace(e);
        }
    }

    [[nodiscard]] bool is_administrator(std::string_view name) const noexcept {
        if (no_authentication_) {
            return true;
        }
        return names_.find(std::string(name)) != names_.end();
    }

  private:
    std::set<std::string> names_{};
    bool no_authentication_{};
};

}  // tateyama::endpoint::common
