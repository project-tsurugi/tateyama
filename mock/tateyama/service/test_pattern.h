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

#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <functional>
#include <optional>
#include <filesystem>
#include <fstream>

namespace tateyama::service {

class test_file {
public:
    test_file(const std::string& name) : name_(name) {
    }
    void clear() {
        std::filesystem::remove(name_);
    }
    void write(std::string_view content) {
        std::ofstream file(name_);
        file << content << std::endl;
        file.close();
    }
private: std::string name_{};
};

class test_pattern {
public:
    test_pattern(bool write, bool clear) : clear_(clear) {
        using namespace std::literals::string_literals;
        blobs_.emplace(13578, std::make_tuple<std::string, std::string, std::string>("blob1"s, "BlobChannelDown1"s, "/tmp/BlobFileDown1"s));
        blobs_.emplace(24680, std::make_tuple<std::string, std::string, std::string>("blob2"s, "BlobChannelDown2"s, "/tmp/BlobFileDown2"s));
        clobs_.emplace(98765, std::make_tuple<std::string, std::string, std::string>("clob"s, "ClobChannelDown"s, "/tmp/ClobFileDown"s));

        if (write) {
            initialize_file();
        }
    }
    ~test_pattern() {
        if (clear_) {
            clear_file();
        }
    }
    std::size_t size() {
        return blobs_.size() + clobs_.size();
    }
    void foreach_blob(std::function<void(std::size_t, std::string&, std::string&, std::string&)> const& f) {
        for(auto&& e: blobs_) {
            auto&& t = e.second;
            f(e.first, std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    }
    void foreach_clob(std::function<void(std::size_t, std::string&, std::string&, std::string&)> const& f) {
        for(auto&& e: clobs_) {
            auto&& t = e.second;
            f(e.first, std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    }
    std::optional<std::string> find(std::size_t id) {
        if (auto it = blobs_.find(id); it != blobs_.end()) {
            return std::get<1>(it->second);
        }
        if (auto it = clobs_.find(id); it != clobs_.end()) {
            return std::get<1>(it->second);
        }
        return std::nullopt;
    }

private:
    bool clear_;
    std::map<std::size_t, std::tuple<std::string, std::string, std::string>> blobs_{};
    std::map<std::size_t, std::tuple<std::string, std::string, std::string>> clobs_{};

    void initialize_file() {
        using namespace std::literals::string_literals;
        foreach_blob([](std::size_t, std::string& column, std::string& channel, std::string& file_name){
            if (!std::filesystem::exists(std::filesystem::status(file_name))) {
                test_file file{file_name};
                file.write("this is a test file of blob, column = "s + column + ", channel = "s + channel);
            }
        });
        foreach_clob([](std::size_t, std::string& column, std::string& channel, std::string& file_name){
            if (!std::filesystem::exists(std::filesystem::status(file_name))) {
                test_file file{file_name};
                file.write("this is a test file of clob, column = "s + column + ", channel = "s + channel);
            }
        });
    }
    void clear_file() {
        foreach_blob([](std::size_t, std::string&, std::string&, std::string& file_name){
            if (std::filesystem::exists(std::filesystem::status(file_name))) {
                test_file file{file_name};
                file.clear();
            }
        });
        foreach_clob([](std::size_t, std::string&, std::string&, std::string& file_name){
            if (std::filesystem::exists(std::filesystem::status(file_name))) {
                test_file file{file_name};
                file.clear();
            }
        });
    }

    };

} // namespace tateyama::mock
