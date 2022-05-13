/*
 * Copyright 2018-2022 tsurugi project.
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

#include <tateyama/api/configuration.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>

namespace tateyama::datastore::resource {

/**
 * @brief datastore resource main object
 */
class core {
public:
    core() = default;

    explicit core(std::shared_ptr<tateyama::api::configuration::whole> cfg);

    bool start();

    bool shutdown(bool force = false);

    std::vector<std::string> list_backup_files();

private:
    std::shared_ptr<tateyama::api::configuration::whole> cfg_{};
};

}
