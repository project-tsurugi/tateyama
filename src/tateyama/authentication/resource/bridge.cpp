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
#include <stdexcept>

#include <glog/logging.h>

#include "authentication_adapter_impl.h"

#include "bridge.h"

namespace tateyama::authentication::resource {

using namespace framework;

bool bridge::setup(environment& env) {
    try {
        if (!authentication_adapter_) {  // In case of test, authentication_adapter_ is pre-set.
            auto auth_conf = env.configuration()->get_section("authentication");

            bool enabled = false;
            auto enabled_option = auth_conf->get<bool>("enabled");
            if (enabled_option) {
                enabled = enabled_option.value();
            }
            if (auto url_option = auth_conf->get<std::string>("url"); url_option) {
                authentication_adapter_ = std::make_unique<authentication_adapter_impl>(enabled, url_option.value());
            } else {
                authentication_adapter_ = std::make_unique<authentication_adapter_impl>(enabled, "");
            }
        }
    } catch (std::runtime_error &ex) {
        LOG(ERROR) << "error in authentication setup: " << ex.what();
        return false;
    }
    return true;
}

} //  namespace tateyama::authentication::resource
