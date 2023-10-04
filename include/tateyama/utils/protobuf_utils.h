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

#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace tateyama::utils {

// Until libprotobuf 3.3, api to manipulate delimited data is not available.
// Here we implemented the corresponding functions on our own.
// TODO use library implementation when migrating to newer one

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

/**
 * @brief retrieve the body in the next delimited part of the input stream
 * @param input the input stream
 * @param clean_eof returns whether seeing clean eof
 * @param out the reference to body
 * @return true if successful
 * @return false on eof or any other error
 */
bool GetDelimitedBodyFromCodedStream(
    google::protobuf::io::CodedInputStream* input,
    bool* clean_eof,
    std::string_view& out
);

/**
 * @brief retrieve the body in the next delimited part of the input stream
 * @param input the input stream
 * @param clean_eof returns whether seeing clean eof
 * @param out the reference to body
 * @return true if successful
 * @return false on eof or any other error
 */
bool GetDelimitedBodyFromZeroCopyStream(
    google::protobuf::io::ZeroCopyInputStream* input,
    bool* clean_eof,
    std::string_view& out
);

/**
 * @brief put the body into stream as the next delimited part
 * @param in the input body binary data
 * @param output the stream to append the body as new delimited part
 * @return true if successful
 * @return false on error
 */
bool PutDelimitedBodyToOstream(
    std::string_view in,
    std::ostream* output
);

/**
 * @brief put the body into stream as the next delimited part
 * @param in the input body binary data
 * @param output the stream to append the body as new delimited part
 * @return true if successful
 * @return false on error
 */
bool PutDelimitedBodyToZeroCopyStream(
    std::string_view in,
    google::protobuf::io::ZeroCopyOutputStream* output
);

/**
 * @brief put the body into stream as the next delimited part
 * @param in the input body binary data
 * @param output the stream to append the body as new delimited part
 * @return true if successful
 * @return false on error
 */
bool PutDelimitedBodyToCodedStream(
    std::string_view in,
    google::protobuf::io::CodedOutputStream* output
);

}

