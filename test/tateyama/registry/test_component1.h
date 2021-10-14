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

namespace ns1 {

class test_component11 : public common::component1 {
public:
    std::string run() override {
        return "test_component11";
    };

    static std::shared_ptr<component1> create() {
        return std::make_shared<test_component11>();
    }
};

register_component(ns1, common::component1, 1_1, ns1::test_component11::create);

class test_component12 : public common::component1 {
public:
    std::string run() override {
        return "test_component12";
    };

    static std::shared_ptr<component1> create() {
        return std::make_shared<test_component12>();
    }
};

}

register_component(ns1, common::component1, 1_2, ns1::test_component12::create);