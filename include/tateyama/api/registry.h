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
#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <string_view>

#define register_component(ns, cls, name, create_func) \
    inline bool ns ## _ ## name ## _entry = ::tateyama::api::registry<cls>::add(#name, (create_func))

namespace tateyama::api {

template <typename T>
class registry {
public:
    using factory_function = std::function<std::shared_ptr<T>()>;
    using factory_map = std::unordered_map<std::string, factory_function>;

    static bool add(std::string_view name, factory_function fn) {
        auto& map = factories();
        if (map.count(std::string{name}) != 0) {
            return false;
        }
        map[std::string(name)] = fn;
        return true;
    }

    static std::shared_ptr<T> create(std::string_view name) {
        auto& map = factories();
        if (map.count(std::string{name}) == 0) {
            return {};
        }
        return map[std::string(name)]();
    }

private:
    static factory_map& factories() {
        static factory_map map{};
        return map;
    }
};

}