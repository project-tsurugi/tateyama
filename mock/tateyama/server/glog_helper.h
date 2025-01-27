/*
 * Copyright 2019-2025 Project Tsurugi.
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

namespace tateyama::server {

// intended to be included from backend.cpp only once
static void setup_glog(tateyama::api::configuration::whole* conf) {
    auto* glog_section = conf->get_section("glog");

    // logtostderr
    if (auto logtostderr_env = getenv("GLOG_logtostderr"); logtostderr_env) {
        FLAGS_logtostderr = true;
    } else {
        auto logtostderr = glog_section->get<bool>("logtostderr");
        if (logtostderr) {
            FLAGS_logtostderr = logtostderr.value();
        }
    }

    // stderrthreshold
    if (auto stderrthreshold_env = getenv("GLOG_stderrthreshold"); stderrthreshold_env) {
        FLAGS_stderrthreshold = static_cast<::google::int32>(strtol(stderrthreshold_env, nullptr, 10));
    } else {
        auto stderrthreshold = glog_section->get<int>("stderrthreshold");
        if (stderrthreshold) {
            FLAGS_stderrthreshold = stderrthreshold.value();
        }
    }

    // minloglevel
    if (auto minloglevel_env = getenv("GLOG_minloglevel"); minloglevel_env) {
        FLAGS_minloglevel = static_cast<::google::int32>(strtol(minloglevel_env, nullptr, 10));
    } else {
        auto minloglevel = glog_section->get<int>("minloglevel");
        if (minloglevel) {
            FLAGS_minloglevel = minloglevel.value();
        }
    }

    // log_dir
    if (auto log_dir_env = getenv("GLOG_log_dir"); log_dir_env) {
        FLAGS_log_dir=log_dir_env;
    } else {
        auto log_dir = glog_section->get<std::filesystem::path>("log_dir");
        if (log_dir) {
            FLAGS_log_dir=log_dir.value().string();
        }
    }

    // max_log_size
    if (auto max_log_size_env = getenv("GLOG_max_log_size"); max_log_size_env) {
        FLAGS_max_log_size = static_cast<::google::int32>(strtol(max_log_size_env, nullptr, 10));
    } else {
        auto max_log_size = glog_section->get<int>("max_log_size");
        if (max_log_size) {
            FLAGS_max_log_size = max_log_size.value();
        }
    }

    // v
    if (auto v_env = getenv("GLOG_v"); v_env) {
        FLAGS_v = static_cast<::google::int32>(strtol(v_env, nullptr, 10));
    } else {
        auto v = glog_section->get<int>("v");
        if (v) {
            FLAGS_v = v.value();
        }
    }

    // logbuflevel
    if (auto logbuflevel_env = getenv("GLOG_logbuflevel"); logbuflevel_env) {
        FLAGS_logbuflevel = static_cast<::google::int32>(strtol(logbuflevel_env, nullptr, 10));
    } else {
        auto logbuflevel = glog_section->get<int>("logbuflevel");
        if (logbuflevel) {
            FLAGS_logbuflevel = logbuflevel.value();
        }
    }

    google::InitGoogleLogging("mock_server");
    google::InstallFailureSignalHandler();
}

}  // tateyama::server
