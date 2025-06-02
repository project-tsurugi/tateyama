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

#include <string>
#include <optional>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <iterator>
#include <sstream>
#include <filesystem>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include <glog/logging.h>
#include <tateyama/logging.h>
#include <tateyama/logging_helper.h>

namespace tateyama::api::configuration {

class whole;

class section {
public:
    section (boost::property_tree::ptree& pt, boost::property_tree::ptree& dt, whole* parent, bool default_required) : property_tree_(pt), default_tree_(dt), default_valid_(true), parent_(parent), default_required_(default_required) {}
    section (boost::property_tree::ptree& dt, whole* parent, bool default_required) : property_tree_(dt), default_tree_(dt), default_valid_(false), parent_(parent), default_required_(default_required) {}
    template<typename T>
    [[nodiscard]] inline std::optional<T> get(std::string_view n) const {
        auto name = std::string(n);
        if (auto it = property_tree_.find(name) ; it != property_tree_.not_found()) {
            auto value = it->second.data();
            if (!value.empty()) {
                try {
                    auto rv = boost::lexical_cast<T>(value);
                    VLOG_LP(log_trace) << "property " << name << " has found in tsurugi.ini and is " << rv;
                    return rv;
                } catch (boost::bad_lexical_cast &) {
                    LOG_LP(ERROR) << "value of " << name << " is '" << value << "', which can not be converted to the type specified";
                    throw std::runtime_error("the parameter string can not be converted to the type specified");
                }
            }
            return std::nullopt;
        }
        if (default_valid_) {
            if (auto it = default_tree_.find(name) ; it != default_tree_.not_found()) {
                auto value = it->second.data();
                if (!value.empty()) {
                    try {
                        auto rv = boost::lexical_cast<T>(value);
                        VLOG_LP(log_trace) << "property " << name << " has found in default and is " << rv;
                        return rv;
                    } catch (boost::bad_lexical_cast &) {
                        VLOG_LP(log_trace) << "value of " << name << " is '" << value << "', which can not be converted to the type specified";
                        throw std::runtime_error("the parameter string can not be converted to the type specified");
                    }
                }
                return std::nullopt;
            }
        }

        // To support hidden configuration parameter, comment out the error msg for now.
        // if (default_required_) {
        //     LOG_LP(ERROR) << "both tree did not have such property: " << name;
        // }
        return std::nullopt;
    }

    // just to suppress clang clang-diagnostic-unused-private-field error
    inline void dummy_message_output(std::string_view name) const {  
        if (default_required_) {
            LOG_LP(ERROR) << "both tree did not have such property: " << name;
        }
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
    bool default_valid_;
    whole* parent_;
    bool default_required_;
};

/**
 * @brief inequality comparison operator
 */
inline bool operator!=(section const& a, section const& b) noexcept {
    return !(a == b);
}

template<>
[[nodiscard]] inline std::optional<std::string> section::get<std::string>(std::string_view n) const {
        auto name = std::string(n);
        if (auto it = property_tree_.find(name) ; it != property_tree_.not_found()) {
            return it->second.data();
        }
        if (default_valid_) {
            if (auto it = default_tree_.find(name) ; it != default_tree_.not_found()) {
                return it->second.data();
            }
        }

        // To support hidden configuration parameter, comment out the error msg for now.
        // LOG_LP(ERROR) << "both tree did not have such property: " << name;
        return std::nullopt;
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
        LOG_LP(ERROR) << "value of " << name << " is '" << str << "', which is not boolean";
        throw std::runtime_error("the parameter string can not be converted to bool");
    }
    return std::nullopt;
}


class whole {
public:
    whole(std::string_view file_name, std::string_view default_property) {
        file_ = std::filesystem::path(std::string(file_name));
        std::ifstream stream{};
        if (std::filesystem::exists(file_)) {
            property_file_exist_ = true;
            stream = std::ifstream{file_.c_str()};
            initialize(stream, default_property);
        } else {
            vlog_info_ << "cannot find " << file_name << ", thus we use default property only." << std::endl;
            std::stringstream empty_ss{};
            initialize(empty_ss, default_property);
        }
    }
    // this constructor works as if property file exists and its content is provided as istream
    whole(std::istream& content, std::string_view default_property) {
        initialize(content, default_property);
    }
    // default_property can be empty only for test purpose
    explicit whole(std::string_view file_name) : whole(file_name, "") {};
    explicit whole(std::istream& content) : whole(content, "") {};

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

    [[nodiscard]] std::optional<std::filesystem::path> base_path() const {
        return base_path_;
    }
    void base_path(const std::filesystem::path& base_path) {
        base_path_ = base_path;
    }

    /**
     * @brief get directory of the config file
     * @return path of the directory
     */
    std::filesystem::path get_directory() {
        if (property_file_exist_) {
            return file_.parent_path();
        }
        return {""};
    }

    /**
     * @brief get canonical path of the config file
     * @return canonical path of the config file
     */
    std::filesystem::path get_canonical_path() {
        if (property_file_exist_) {
            return std::filesystem::canonical(file_);
        }
        return {""};
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
        return std::all_of(a.property_tree_.begin(), a.property_tree_.end(), [&a, &b](auto&& section) {
            auto& name = section.first;
            auto* pa = a.get_section(name);
            auto* pb = b.get_section(name);
            return *pa == *pb;
        });
    }

    /**
     * @brief show VLOG_LP(log_info) message that was output before InitGoogleLogging()
     */
    void show_vlog_info_message() {
        if (vlog_info_.tellp() != std::streampos(0)) {
            VLOG_LP(log_info) << vlog_info_.str();
            vlog_info_.clear();
        }
    }

private:
    boost::property_tree::ptree property_tree_;
    boost::property_tree::ptree default_tree_;
    std::filesystem::path file_{};
    bool property_file_exist_{};
    bool check_done_{};
    bool default_valid_{};
    std::optional<std::filesystem::path> base_path_{std::nullopt};
    std::stringstream vlog_info_{};

    std::unordered_map<std::string, std::unique_ptr<section>> map_;

    bool check() {
        if (!property_file_exist_ || check_done_ || !default_valid_) {
            return true;
        }
        check_done_ = true;

        bool rv = true;
        BOOST_FOREACH(const boost::property_tree::ptree::value_type &s, property_tree_) {
            auto& section_name = s.first;
            bool default_required = (section_name != "glog");
            try {
                boost::property_tree::ptree& default_section = default_tree_.get_child(section_name);
                BOOST_FOREACH(const boost::property_tree::ptree::value_type &p, s.second) {
                    try {
                        default_section.get<std::string>(p.first);
                    } catch (boost::property_tree::ptree_error &e) {
                        if (default_required) {
                            LOG_LP(ERROR) << "property '" << p.first << "' is not in the '" << section_name << "' section in the default configuration.";
//                          rv = false;  //  FIXME  As a provisional measure, treat as not an error if the property is not in the default configuration.
                        }
                        continue;
                    }
                }
            } catch (boost::property_tree::ptree_error &e) {
                if (default_required) {
                    LOG_LP(ERROR) << "section '" << section_name << "' is not in the default configuration.";
//                  rv = false;  //  FIXME  As a provisional measure, treat as not an error if the property is not in the default configuration.
                }
                continue;
            }
        }
        return rv;
    }

    static std::string_view default_property();

    void initialize(std::istream& content, std::string_view default_property) {
        auto default_conf_string = std::string(default_property);
        if (!default_conf_string.empty()) {
            std::istringstream default_iss(default_conf_string);  // NOLINT
            boost::property_tree::read_ini(default_iss, default_tree_);
            default_valid_ = true;
        } else {
            default_valid_ = false;
        }

        try {
            boost::property_tree::read_ini(content, property_tree_);
        } catch (boost::property_tree::ini_parser_error &e) {
            vlog_info_ << "error reading input, thus we use default property only. msg:" << e.what() << std::endl;
        }
        if (default_valid_) {
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, default_tree_) {
                auto& dt = default_tree_.get_child(v.first);
                bool default_required = (v.first != "glog");
                try {
                    auto& pt = property_tree_.get_child(v.first);
                    map_.emplace(v.first, std::make_unique<section>(pt, dt, this, default_required));
                } catch (boost::property_tree::ptree_error &e) {
                    vlog_info_ << "cannot find " << v.first << " section in the input, thus we use default property only." << std::endl;
                    map_.emplace(v.first, std::make_unique<section>(dt, this, default_required));
                }
            }
            if (!check()) {
                BOOST_PROPERTY_TREE_THROW(boost::property_tree::ptree_error("orphan entry error"));  // NOLINT
            }
        }
    }

    [[nodiscard]] section* get_section_internal(std::string_view n) const {
        auto name = std::string(n);
        if (auto it = map_.find(name); it != map_.end()) {
            VLOG_LP(log_trace) << "configuration of section " << name << " will be used.";
            return it->second.get();
        }
        LOG_LP(ERROR) << "cannot find " << name << " section in the configuration.";
        return nullptr;
    }
};


/**
 * @brief get std::filesystem::epath from configuration file
 * @return the file path from the specified parameter. if the file path is not absolute path (does not begin with '/'), the base path is given at the beginning of the file path.
 */
template<>
[[nodiscard]] inline std::optional<std::filesystem::path> section::get<std::filesystem::path>(std::string_view name) const {
    std::optional<std::string> opt = get<std::string>(name);
    if (opt) {
        const auto& str = opt.value();
        if (str.empty()) {
            return std::filesystem::path{};
        }
        std::filesystem::path ep = str;
        if (ep.is_absolute()) {
            return ep;
        }
        auto bp = parent_->base_path();
        if (bp) {
            return bp.value() / ep;
        }
        LOG_LP(ERROR) << "value of " << name << " is '" << str << "', which is relative path and the the base path is empty";
        throw std::runtime_error("the parameter string is relative path and the base path is empty");
    }
    return std::nullopt;
}

/**
 * @brief inequality comparison operator
 */
inline bool operator!=(whole const& a, whole const& b) noexcept {
    return !(a == b);
}

inline std::shared_ptr<whole> create_configuration(std::string_view file_name = "", std::string_view default_property = "") {
    try {
        return std::make_shared<whole>(file_name, default_property);
    } catch (boost::property_tree::ptree_error &e) {
        LOG_LP(ERROR) << "cannot create configuration, file name is '" << file_name << "'.";
        return nullptr;
    }
}

}  // namespace tateyama::server::config
