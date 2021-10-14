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

#include <memory>
#include "component.h"

class test_component21 : public component2 {
public:
    int run() override {
        return 21;
    };

    static std::shared_ptr<component2> create() {
        return std::make_shared<test_component21>();
    }
};

register_component(component2, 2_1, test_component21::create);

class test_component22 : public component2 {
public:
    int run() override {
        return 22;
    };

    static std::shared_ptr<component2> create() {
        return std::make_shared<test_component22>();
    }
};

register_component(component2, 2_2, test_component22::create);