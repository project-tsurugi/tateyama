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

#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace tateyama::utils {

bool SerializeDelimitedToOstream(
    google::protobuf::MessageLite const& message,
    std::ostream* output
);

bool ParseDelimitedFromZeroCopyStream(
    google::protobuf::MessageLite* message,
    google::protobuf::io::ZeroCopyInputStream* input,
    bool* clean_eof
);

bool ParseDelimitedFromCodedStream(
    google::protobuf::MessageLite* message,
    google::protobuf::io::CodedInputStream* input,
    bool* clean_eof
);

bool SerializeDelimitedToZeroCopyStream(
    google::protobuf::MessageLite const& message,
    google::protobuf::io::ZeroCopyOutputStream* output
);

bool SerializeDelimitedToCodedStream(
    google::protobuf::MessageLite const& message,
    google::protobuf::io::CodedOutputStream* output
);

// below are the original functions

bool GetDelimitedBodyFromCodedStream(
    google::protobuf::io::CodedInputStream* input,
    bool* clean_eof,
    std::string_view& out
);

bool GetDelimitedBodyFromZeroCopyStream(
    google::protobuf::io::ZeroCopyInputStream* input,
    bool* clean_eof,
    std::string_view& out
);
}

