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

#include <string>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <iterator>
#include <sstream>
#include <cstdlib>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>

#include <glog/logging.h>
#include <tateyama/logging.h>
#include <tateyama/detail/default_configuration.h>

namespace tateyama::api::configuration {

class section {
public:
    section (boost::property_tree::ptree& pt, boost::property_tree::ptree& dt) : property_tree_(pt), default_tree_(dt) {}
    explicit section (boost::property_tree::ptree& dt) : property_tree_(dt), default_tree_(dt) {}
    template<typename T>
    inline bool get(std::string const& name, T& rv) const {
        try {
            rv = property_tree_.get<T>(name, default_tree_.get<T>(name));
            VLOG(log_trace) << "property " << name << " is " << rv;
            return true;
        } catch (boost::property_tree::ptree_error &e) {
            LOG(ERROR) << "both tree: " << e.what();
            return false;
        }
    }

private:
    boost::property_tree::ptree& property_tree_;
    boost::property_tree::ptree& default_tree_;
};

template<>
inline bool section::get<bool>(std::string const& name, bool& rv) const {
    using boost::algorithm::iequals;

    std::string str;
    if (!get<std::string>(name, str)) {
        return false;
    }
    if (iequals(str, "true") || iequals(str, "yes") || str == "1") {
        rv = true;
        return true;
    }
    if (iequals(str, "false") || iequals(str, "no") || str == "0") {
        rv = false;
        return true;
    }
    LOG(ERROR) << "value of " << name << " is '" << str << "', which is not boolean";
    return false;
}


class whole {
public:
    static constexpr char property_flename[] = "tsurugi.ini";  // NOLINT

    explicit whole(std::string const& directory) {
        if (!directory.empty()) {
            directory_ = directory;
        } else {
            auto env = getenv("TGDIR");
            if (env != nullptr) {
                directory_ = std::string(getenv("TGDIR"));
            } else {
                directory_ = std::string("");
                property_file_absent_ = true;
            }
        }
        try {
            auto default_conf_string = std::string(default_configuration);
            std::istringstream default_iss(default_conf_string);  // NOLINT
            boost::property_tree::read_ini(default_iss, default_tree_);
        } catch (boost::property_tree::ini_parser_error &e) {
            LOG(ERROR) << "default tree: " << e.what();
            BOOST_PROPERTY_TREE_THROW(e);  // NOLINT
        }
        if (!property_file_absent_) {
            try {
                boost::property_tree::read_ini((directory_ / boost::filesystem::path(property_flename)).string(), property_tree_);  // NOLINT
            } catch (boost::property_tree::ini_parser_error &e) {
                VLOG(log_info) << "cannot find " << e.filename() << ", thus we use default property only.";
                property_file_absent_ = true;
            }
        }
        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, default_tree_) {
            auto& dt = default_tree_.get_child(v.first);
            if (!property_file_absent_) {
                try {
                    auto& pt = property_tree_.get_child(v.first);
                    map_.emplace(v.first, std::make_unique<section>(pt, dt));
                } catch (boost::property_tree::ptree_error &e) {
                    VLOG(log_info) << "cannot find " << v.first << " section in the property file, thus we use default property only.";
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
    ~whole() {
        map_.clear();
    }

    whole(whole const& other) = delete;
    whole& operator=(whole const& other) = delete;
    whole(whole&& other) noexcept = delete;
    whole& operator=(whole&& other) noexcept = delete;

    section* get_section(const std::string&& name) {
        if (auto it = map_.find(name); it != map_.end()) {
            VLOG(log_trace) << "configurateion of section " << name << " will be used.";
            return map_[name].get();
        }
        LOG(ERROR) << "cannot find " << name << " section in the configuration.";
        return nullptr;
    }
    boost::filesystem::path& get_directory() {
        return directory_;
    }

private:
    boost::property_tree::ptree property_tree_;
    boost::property_tree::ptree default_tree_;
    boost::filesystem::path directory_{};
    bool property_file_absent_{};

    std::unordered_map<std::string, std::unique_ptr<section>> map_;

    bool check() {
        if (property_file_absent_) {
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

inline std::shared_ptr<whole> create_configuration(std::string const& dir) {
    try {
        return std::make_shared<whole>(dir);
    } catch (boost::property_tree::ptree_error &e) {
        return nullptr;
    }
}

}  // namespace tateyama::server::config
