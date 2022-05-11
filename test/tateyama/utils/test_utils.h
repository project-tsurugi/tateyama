/*
 * Copyright 2018-2021 tsurugi project.
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

#include "temporary_folder.h"

namespace tateyama::test {

class test_utils {
public:
    [[nodiscard]] std::string path() const {
        return temporary_.path();
    }

    void set_dbpath(api::configuration::whole& cfg) {
        auto* section = cfg.get_section("data_store");
        BOOST_ASSERT(section != nullptr); //NOLINT
        section->set("log_location", path());
    }

protected:
    temporary_folder temporary_{};
};

}