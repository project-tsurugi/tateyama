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
#include <cmath>

#include <glog/logging.h>

#include "authentication_adapter_impl.h"

#include "bridge.h"

namespace tateyama::authentication::resource {

using namespace framework;

bool bridge::setup(environment& env) {
    try {
        if (!authentication_adapter_) {  // In case of test, where authentication_adapter_ is pre-set.
            auto auth_conf = env.configuration()->get_section("authentication");

            if (auto enabled_option = auth_conf->get<bool>("enabled"); enabled_option) {
                bool enabled = enabled_option.value();
                if (enabled) {
                    if (auto url_option = auth_conf->get<std::string>("url"); url_option) {
                        if (auto request_timeout_opt = auth_conf->get<double>("request_timeout"); request_timeout_opt) {
                            refresh_ = static_cast<std::chrono::milliseconds>(lround(request_timeout_opt.value() * 1000));
                            authentication_adapter_ = std::make_unique<authentication_adapter_impl>(enabled, url_option.value());
                        } else {
                            LOG(ERROR) << "cannot find authentication.request_timeout in the configuration";
                            return false;
                        }
                    } else {
                        LOG(ERROR) << "cannot find authentication.url in the configuration";
                        return false;
                    }
                } else {
                    authentication_adapter_ = std::make_unique<authentication_adapter_impl>(enabled, "");
                }
            } else {
                LOG(ERROR) << "cannot find authentication.enabled in the configuration";
                return false;
            }
        }
    } catch (std::runtime_error &ex) {
        LOG(ERROR) << "error in authentication setup: " << ex.what();
        return false;
    }
    return true;
}

} //  namespace tateyama::authentication::resource
