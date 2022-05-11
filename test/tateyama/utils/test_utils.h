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
        auto* dscfg = cfg.get_section("data_store");
        BOOST_ASSERT(dscfg != nullptr); //NOLINT
        dscfg->set("log_location", path());

        auto* ipccfg = cfg.get_section("ipc_endpoint");
        BOOST_ASSERT(ipccfg != nullptr); //NOLINT
        auto p = boost::filesystem::unique_path();
        ipccfg->set("database_name", p.string());

        auto* stmcfg = cfg.get_section("stream_endpoint");
        BOOST_ASSERT(stmcfg != nullptr); //NOLINT
        auto i = ++integer_src_;
        stmcfg->set("port", std::to_string(12346+i));
    }

protected:
    temporary_folder temporary_{};
    static inline std::atomic_size_t integer_src_{0};
};

}