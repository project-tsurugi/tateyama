/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <string_view>
#include <glog/logging.h>
#include <tateyama/logging.h>

#include <tateyama/status/resource/bridge.h>
#include "tateyama/session/resource/bridge.h"

namespace tateyama::session::resource {

void bridge::start_print_thread() {
    if (status_info_) {
        print_thread_ = std::thread([this](){
            while (true) {
                status_info_->wait_for_session_list([this](){return shutdown_;});
                if (shutdown_) { return; }
                print_session_list_to_log();
            }
        });
    }
}

void bridge::print_session_list_to_log() {
    LOG(INFO) << "======== remaining session begin ========";

    std::size_t n{};
    sessions_core_impl_.container_.foreach([&n](const std::shared_ptr<session_context>& entry){
        auto& info = entry->info();
        std::string user_name{};
        if (auto name_opt = info.username(); name_opt) {
            user_name = name_opt.value();
        }
        
        LOG(INFO) << "session id = :" << info.id() <<
            ", label = \"" << info.label() <<
            "\", application name = \"" << info.application_name() <<
            "\", user name = \"" << user_name <<
            "\", connection type = \"" << info.connection_type_name() <<
            "\", connection info = \"" << info.connection_information() << "\"";

        n++;                     
    });

    LOG(INFO) << "======== remaining session end, total " << n << (n == 1 ? " session" : " sessions") << " ========";
}

} // namespace
