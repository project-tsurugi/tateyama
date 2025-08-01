/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <sstream>

#include "ipc_test_utils.h"

namespace tateyama::endpoint::ipc {

class ipc_test_env: public test_utils::utility {
public:
    void setup(std::string config = "") {
        temporary_.prepare();
        std::stringstream ss{};
        ss << config;
        cfg_ = std::make_shared<tateyama::api::configuration::whole>(ss, tateyama::test_utils::default_configuration_for_tests);
        set_dbpath(*cfg_);
        get_ipc_max_session(cfg_, ipc_max_session_);
    }

    void teardown() {
        temporary_.clean();
    }

    std::shared_ptr<tateyama::api::configuration::whole>& config() {
        return cfg_;
    }

    int ipc_max_session() {
        return ipc_max_session_;
    }

protected:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_ { };
    int ipc_max_session_ { };
};

} // namespace tateyama::endpoint::ipc
