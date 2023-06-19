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
#include <boost/filesystem/operations.hpp>

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

        // To support hidden configuration parameter, comment out the error msg for now.
        // LOG(ERROR) << "both tree did not have such property: " << name;
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

    /**
     * @brief equality comparison operator
     */
    friend bool operator==(section const& a, section const& b) noexcept {
        return a.property_tree_ == b.property_tree_;
    }
private:
    boost::property_tree::ptree& property_tree_;
    boost::property_tree::ptree& default_tree_;
};

/**
 * @brief inequality comparison operator
 */
inline bool operator!=(section const& a, section const& b) noexcept {
    return !(a == b);
}

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
    whole(std::string_view file_name, std::string_view default_property) {
        file_ = boost::filesystem::path(std::string(file_name));
        std::ifstream stream{};
        if (boost::filesystem::exists(file_)) {
            property_file_exist_ = true;
            stream = std::ifstream{file_.c_str()};
        } else {
            VLOG(log_info) << "cannot find " << file_name << ", thus we use default property only.";
        }
        initialize(stream, default_property);
    }
    // this constructor works as if property file exists and its content is provided as istream
    whole(std::istream& content, std::string_view default_property) : property_file_exist_(true) {
        initialize(content, default_property);
    }
    explicit whole(std::string_view file_name) : whole(file_name, default_property()) {
    }
    explicit whole(std::istream& content) : whole(content, default_property()) {
    }

    ~whole() = default;

    whole(whole const& other) = delete;
    whole& operator=(whole const& other) = delete;
    whole(whole&& other) noexcept = delete;
    whole& operator=(whole&& other) noexcept = delete;

    [[nodiscard]] section const* get_section(std::string_view n) const {
        return get_section_internal(n);
    }

    [[nodiscard]] section* get_section(std::string_view n) {
        return get_section_internal(n);
    }

    /**
     * @brief get directory of the config file
     * @return path of the directory
     */
    boost::filesystem::path get_directory() {
        if (property_file_exist_) {
            return file_.parent_path();
        }
        return boost::filesystem::path("");
    }

    /**
     * @brief get canonical path of the config file
     * @return canonical path of the config file
     */
    boost::filesystem::path get_canonical_path() {
        if (property_file_exist_) {
            return boost::filesystem::canonical(file_);
        }
        return boost::filesystem::path("");
    }


    /**
     * @brief get the ptree containing the properties
     * @return ptree containing the properties specified by the config file
     */
    [[nodiscard]] boost::property_tree::ptree& get_ptree() {
        return property_tree_;
    }

    /**
     * @brief equality comparison operator
     */
    friend bool operator==(whole const& a, whole const& b) noexcept {
        if(a.property_tree_ != b.property_tree_) {
            return false;
        }
        for(auto&& section : a.property_tree_) {
            auto& name = section.first;
            auto* pa = a.get_section(name);
            auto* pb = b.get_section(name);
            if(*pa != *pb) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief default property (should be moved to tateyama-bootstrap)
     */
    static std::string_view default_property();

private:
    boost::property_tree::ptree property_tree_;
    boost::property_tree::ptree default_tree_;
    boost::filesystem::path file_{};
    bool property_file_exist_{};
    bool check_done_{};

    std::unordered_map<std::string, std::unique_ptr<section>> map_;

    bool check() {
        if (!property_file_exist_ || check_done_) {
            return true;
        }
        check_done_ = true;

        bool rv = true;
        BOOST_FOREACH(const boost::property_tree::ptree::value_type &s, property_tree_) {
            try {
                boost::property_tree::ptree& default_section = default_tree_.get_child(s.first);
                BOOST_FOREACH(const boost::property_tree::ptree::value_type &p, s.second) {
                    try {
                        default_section.get<std::string>(p.first);
                    } catch (boost::property_tree::ptree_error &e) {
                        LOG(ERROR) << "property '" << p.first << "' is not in the '" << s.first << "' section in the default configuration.";
//                        rv = false;  //  FIXME  As a provisional measure, treat as not an error if the property is not in the default configuration.
                        continue;
                    }
                }
            } catch (boost::property_tree::ptree_error &e) {
                LOG(ERROR) << "section '" << s.first << "' is not in the default configuration.";
//                rv = false;  //  FIXME  As a provisional measure, treat as not an error if the property is not in the default configuration.
                continue;
            }
        }
        return rv;
    }

    void initialize(std::istream& content, std::string_view default_property) {
        auto default_conf_string = std::string(default_property);
        if (!default_conf_string.empty()) {
            std::istringstream default_iss(default_conf_string);  // NOLINT
            boost::property_tree::read_ini(default_iss, default_tree_);
        }

        try {
            boost::property_tree::read_ini(content, property_tree_);
        } catch (boost::property_tree::ini_parser_error &e) {
            VLOG(log_info) << "error reading input, thus we use default property only. msg:" << e.what();
        }
        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, default_tree_) {
            auto& dt = default_tree_.get_child(v.first);
            if (property_file_exist_) {
                try {
                    auto& pt = property_tree_.get_child(v.first);
                    map_.emplace(v.first, std::make_unique<section>(pt, dt));
                } catch (boost::property_tree::ptree_error &e) {
                    VLOG(log_info) << "cannot find " << v.first << " section in the input, thus we use default property only.";
                    map_.emplace(v.first, std::make_unique<section>(dt));
                }
            } else {
                map_.emplace(v.first, std::make_unique<section>(dt));
            }
        }
        if (!check()) {
            BOOST_PROPERTY_TREE_THROW(boost::property_tree::ptree_error("orphan entry error"));  // NOLINT
        }
    }

    [[nodiscard]] section* get_section_internal(std::string_view n) const {
        auto name = std::string(n);
        if (auto it = map_.find(name); it != map_.end()) {
            VLOG(log_trace) << "configuration of section " << name << " will be used.";
            return it->second.get();
        }
        LOG(ERROR) << "cannot find " << name << " section in the configuration.";
        return nullptr;
    }
};

/**
 * @brief inequality comparison operator
 */
inline bool operator!=(whole const& a, whole const& b) noexcept {
    return !(a == b);
}

inline std::shared_ptr<whole> create_configuration(std::string_view file_name = "", std::string_view default_property = whole::default_property()) {
    try {
        return std::make_shared<whole>(file_name, default_property);
    } catch (boost::property_tree::ptree_error &e) {
        LOG(ERROR) << "cannot create configuration, file name is '" << file_name << "'.";
        return nullptr;
    }
}

}  // namespace tateyama::server::config
