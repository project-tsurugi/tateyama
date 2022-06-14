/*
 * Copyright 2022-2022 tsurugi project.
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

#include <string>
#include <optional>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <iterator>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>

#include <glog/logging.h>
#include <tateyama/logging.h>

namespace tateyama::api::configuration {

class section {
public:
    section (boost::property_tree::ptree& pt, boost::property_tree::ptree& dt) : property_tree_(pt), default_tree_(dt) {}
    explicit section (boost::property_tree::ptree& dt) : property_tree_(dt), default_tree_(dt) {}
    template<typename T>
    [[nodiscard]] inline std::optional<T> get(std::string_view n) const {
        auto name = std::string(n);
        if (auto it = property_tree_.find(name) ; it != property_tree_.not_found()) {
            auto rv = boost::lexical_cast<T>(it->second.data());
            VLOG(log_trace) << "property " << name << " has found in tsurugi.ini and is " << rv;
            return rv;
        }
        if (auto it = default_tree_.find(name) ; it != default_tree_.not_found()) {
            auto value = it->second.data();
            if (!value.empty()) {
                auto rv = boost::lexical_cast<T>(value);
                VLOG(log_trace) << "property " << name << " has found in default and is " << rv;
                return rv;
            }
            VLOG(log_trace) << "property " << name << " exists but the value is empty";
            return std::nullopt;
        }
        LOG(ERROR) << "both tree did not have such property: " << name;
        return std::nullopt;
    }

    /**
     * @brief set property value for the given key
     * @param n key
     * @param v value
     * @return true if new property is added
     * @return true if existing property value is updated
     */
    inline bool set(std::string_view n, std::string_view v) {
        auto name = std::string(n);
        auto value = std::string{v};
        if (auto it = property_tree_.find(name) ; it != property_tree_.not_found()) {
            it->second.data() = value;
            return false;
        }
        property_tree_.push_back({name, boost::property_tree::ptree{value}});
        return true;
    }

private:
    boost::property_tree::ptree& property_tree_;
    boost::property_tree::ptree& default_tree_;
};

template<>
[[nodiscard]] inline std::optional<bool> section::get<bool>(std::string_view name) const {
    using boost::algorithm::iequals;

    std::optional<std::string> opt = get<std::string>(name);
    if (opt) {
        auto str = opt.value();
        if (iequals(str, "true") || iequals(str, "yes") || str == "1") {
            return true;
        }
        if (iequals(str, "false") || iequals(str, "no") || str == "0") {
            return false;
        }
        LOG(ERROR) << "value of " << name << " is '" << str << "', which is not boolean";
        return std::nullopt;
    }
    return std::nullopt;
}


class whole {
public:
    static constexpr char property_filename[] = "tsurugi.ini";  // NOLINT

    explicit whole(std::string_view file_name);
    ~whole() = default;

    whole(whole const& other) = delete;
    whole& operator=(whole const& other) = delete;
    whole(whole&& other) noexcept = delete;
    whole& operator=(whole&& other) noexcept = delete;

    [[nodiscard]] section* get_section(std::string_view n) {
        auto name = std::string(n);
        if (auto it = map_.find(name); it != map_.end()) {
            VLOG(log_trace) << "configuration of section " << name << " will be used.";
            return map_[name].get();
        }
        LOG(ERROR) << "cannot find " << name << " section in the configuration.";
        return nullptr;
    }

    /**
     * @brief get directory of the config file
     * @return path of the directory
     */
    boost::filesystem::path get_directory() {
        return file_.parent_path();
    }

private:
    boost::property_tree::ptree property_tree_;
    boost::property_tree::ptree default_tree_;
    boost::filesystem::path file_{};
    bool property_file_exist_{};

    std::unordered_map<std::string, std::unique_ptr<section>> map_;

    bool check() {
        if (!property_file_exist_) {
            return true;
        }

        BOOST_FOREACH(const boost::property_tree::ptree::value_type &s, property_tree_) {
            try {
                boost::property_tree::ptree& default_section = default_tree_.get_child(s.first);
                BOOST_FOREACH(const boost::property_tree::ptree::value_type &p, s.second) {
                    try {
                        default_section.get<std::string>(p.first);
                    } catch (boost::property_tree::ptree_error &e) {
                        LOG(ERROR) << "property '" << p.first << "' is not in the '" << s.first << "' section in the default configuration.";
//                        return false;  //  FIXME  As a provisional measure, continue processing even if this error occurs.
                        return true;
                    }
                }
            } catch (boost::property_tree::ptree_error &e) {
                LOG(ERROR) << "section '" << s.first << "' is not in the default configuration.";
//                return false;  // FIXME  As a provisional measure, continue processing even if this error occurs.
                return true;
            }
        }
        return true;
    }
};

inline std::shared_ptr<whole> create_configuration(std::string_view file_name = "") {
    try {
        return std::make_shared<whole>(file_name);
    } catch (boost::property_tree::ptree_error &e) {
        LOG(ERROR) << "cannot create configuration, file name is '" << file_name << "'.";
        return nullptr;
    }
}

}  // namespace tateyama::server::config
