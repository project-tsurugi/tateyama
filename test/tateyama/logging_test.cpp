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
#include <tateyama/logging.h>

#include <glog/logging.h>

#include <gtest/gtest.h>

namespace tateyama {

using namespace std::literals::string_literals;

class logging_test : public ::testing::Test {

};

using namespace std::string_view_literals;

TEST_F(logging_test, basic) {
    // verify log messages manually on console
    LOG(INFO) << "LOG(INFO)";
    LOG(WARNING) << "LOG(WARNING)";
    LOG(ERROR) << "LOG(ERROR)";
}

TEST_F(logging_test, fatal) {
    ASSERT_DEATH({ LOG(FATAL) << "LOG(FATAL)"; }, "FATAL");
}

TEST_F(logging_test, event_logging) {
    // verify log messages manually on console
    FLAGS_v = 50;
    VLOG(log_error) << "VLOG(log_error)";
    VLOG(log_warning) << "VLOG(log_warning)";
    VLOG(log_info) << "VLOG(log_info)";
    VLOG(log_debug) << "VLOG(log_debug)";
    VLOG(log_trace) << "VLOG(log_trace)";
}

TEST_F(logging_test, invisibleevent_logging) {
    // verify log messages disappear on console
    FLAGS_v = 0;
    VLOG(log_error) << "VLOG(log_error)";
    VLOG(log_warning) << "VLOG(log_warning)";
    VLOG(log_info) << "VLOG(log_info)";
    VLOG(log_debug) << "VLOG(log_debug)";
    VLOG(log_trace) << "VLOG(log_trace)";
}


}
