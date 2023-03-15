/*
 * Copyright 2018-2020 tsurugi project.
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
#include <tateyama/api/configuration.h>

#include <boost/filesystem.hpp>

namespace tateyama::api::configuration {

namespace details {

static constexpr std::string_view default_configuration {  // NOLINT
    "[sql]\n"
        "thread_pool_size=5\n"
        "lazy_worker=false\n"
        "prepare_qa_tables=false\n"
        "prepare_phone_bill_tables=false\n"
        "enable_index_join=false\n"
        "stealing_enabled=true\n"
        "default_partitions=5\n"

    "[ipc_endpoint]\n"
        "database_name=tateyama\n"
        "threads=104\n"
        "datachannel_buffer_size=64\n"

    "[stream_endpoint]\n"
        "port=12345\n"
        "threads=104\n"

    "[fdw]\n"
        "name=tateyama\n"
        "threads=104\n"

    "[datastore]\n"
        "log_location=\n"
        "logging_max_parallelism=112\n"

    "[cc]\n"
        "epoch_duration=40000\n"

};

} // namespace details


void whole::initialize(std::istream& content) {
    // default configuration
    try {
        auto default_conf_string = std::string(details::default_configuration);
        std::istringstream default_iss(default_conf_string);  // NOLINT
        boost::property_tree::read_ini(default_iss, default_tree_);
    } catch (boost::property_tree::ini_parser_error &e) {
        LOG(ERROR) << "default tree: " << e.what();
        BOOST_PROPERTY_TREE_THROW(e);  // NOLINT
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

whole::whole(std::string_view file_name) {
    file_ = boost::filesystem::path(std::string(file_name));
    std::ifstream stream{};
    if (boost::filesystem::exists(file_)) {
        property_file_exist_ = true;
        stream = std::ifstream{file_.c_str()};
    } else {
        VLOG(log_info) << "cannot find " << file_name << ", thus we use default property only.";
    }
    initialize(stream);
}

whole::whole(std::istream& content) :
    // this constructor works as if property file exists and its content is provided as istream
    property_file_exist_(true)
{
    initialize(content);
}

} // tateyama::api::configuration
