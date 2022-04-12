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
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>

#include <glog/logging.h>
#include <tateyama/logging.h>
#include <tateyama/detail/default_configuration.h>

#define reflect_configuration(cfg, tree, name, type) (cfg)->name( (tree).get<type>( #name ) )  // NOLINT

namespace tateyama::api::configuration {

class section {
public:
    section (boost::property_tree::ptree& pt, boost::property_tree::ptree& dt) : property_tree_(pt), default_tree_(dt) {}
    explicit section (boost::property_tree::ptree& dt) : property_tree_(dt), default_tree_(dt) {}
    template<typename T>
    inline T get(std::string&& name) const {
        try {
            auto rv = property_tree_.get<T>(name, default_tree_.get<T>(name));
            VLOG(log_trace) << "property " << name << " is " << rv;
            return rv;
        } catch (boost::property_tree::ptree_error &e) {
            VLOG(log_error) << "both tree: " << e.what() << ", thus this program aborts intentionally.";
            std::abort();
        }
    }

private:
    boost::property_tree::ptree& property_tree_;
    boost::property_tree::ptree& default_tree_;
};

template<>
inline bool section::get<bool>(std::string&& name) const {
    using boost::algorithm::iequals;

    auto str = get<std::string>(std::move(name));
    if (iequals(str, "true") || iequals(str, "yes") || str == "1") {
        return true;
    }
    if (iequals(str, "false") || iequals(str, "no") || str == "0") {
        return false;
    }
    VLOG(log_error) << "it is not boolean";
    std::abort();
}


class whole {
public:
    static constexpr char property_flename[] = "tsurugi.ini";  // NOLINT

    explicit whole(const std::string& directory) {
        boost::filesystem::path dir = directory;
        if (!directory.empty()) {
            dir = directory;
        } else {
            dir = std::string(getenv("TGDIR"));
        }
        try {
            std::istringstream default_iss(default_configuration);  // NOLINT
            boost::property_tree::read_ini(default_iss, default_tree_);
        } catch (boost::property_tree::ini_parser_error &e) {
            VLOG(log_error) << "default tree: " << e.what() << ", thus this program aborts intentionally.";
            std::abort();
        }
        try {
            boost::property_tree::read_ini((dir / boost::filesystem::path(property_flename)).string(), property_tree_);  // NOLINT
        } catch (boost::property_tree::ini_parser_error &e) {
            VLOG(log_info) << "cannot use " << e.filename() << ", thus we use default property file only.";
            property_file_absent_ = true;
        }
    }
    whole() : whole("") {}
    ~whole() {
        map_.clear();
    }

    whole(whole const& other) = delete;
    whole& operator=(whole const& other) = delete;
    whole(whole&& other) noexcept = delete;
    whole& operator=(whole&& other) noexcept = delete;
    
    section& get_section(const std::string&& name) {
        VLOG(log_trace) << "property section " << name << " is requested.";
        if (auto it = map_.find(name); it != map_.end()) {
            return *map_[name];
        }
        try {
            auto& dt = default_tree_.get_child(name);
            if (!property_file_absent_) {
                try {
                    auto& pt = property_tree_.get_child(name);
                    map_.emplace(name, std::make_unique<section>(pt, dt));
                    return *map_[name];
                } catch (boost::property_tree::ptree_error &e) {
                    VLOG(log_info) << "cannot use " << name << " section in the property file, thus we use default property only";
                    map_.emplace(name, std::make_unique<section>(dt));
                    return *map_[name];
                }
            } else {
                map_.emplace(name, std::make_unique<section>(dt));
                return *map_[name];
            }
        } catch (boost::property_tree::ptree_error &e) {
            VLOG(log_error) << "cannot find " << name << " section in the default property file, thus this program aborts intentionally.";
            std::abort();
        }
    }

private:
    boost::property_tree::ptree property_tree_;
    boost::property_tree::ptree default_tree_;
    bool property_file_absent_{};

    std::unordered_map<std::string, std::unique_ptr<section>> map_;
};

}  // namespace tateyama::server::config
