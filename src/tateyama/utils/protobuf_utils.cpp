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
#include <tateyama/utils/protobuf_utils.h>

#include <ostream>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>

namespace tateyama::utils {

bool ParseDelimitedFromZeroCopyStream(
    google::protobuf::MessageLite* message,
    google::protobuf::io::ZeroCopyInputStream* input,
    bool* clean_eof
) {
    google::protobuf::io::CodedInputStream coded_input(input);
    return ParseDelimitedFromCodedStream(message, &coded_input, clean_eof);
}

bool ParseDelimitedFromCodedStream(
    google::protobuf::MessageLite* message,
    google::protobuf::io::CodedInputStream* input,
    bool* clean_eof
) {
    if (clean_eof != nullptr) *clean_eof = false;
    int start = input->CurrentPosition();

    // Read the size.
    google::protobuf::uint32 size{};
    if (!input->ReadVarint32(&size)) {
        if (clean_eof != nullptr) *clean_eof = input->CurrentPosition() == start;
        return false;
    }

    // Get the position after any size bytes have been read (and only the message
    // itself remains).
    int position_after_size = input->CurrentPosition();

    // Tell the stream not to read beyond that size.
    google::protobuf::io::CodedInputStream::Limit limit = input->PushLimit(size);

    // Parse the message.
    if (!message->MergeFromCodedStream(input)) return false;
    if (!input->ConsumedEntireMessage()) return false;
    if (input->CurrentPosition() - position_after_size != static_cast<int>(size)) {
        return false;
    }

    // Release the limit.
    input->PopLimit(limit);

    return true;
}

bool SerializeDelimitedToZeroCopyStream(
    google::protobuf::MessageLite const& message,
    google::protobuf::io::ZeroCopyOutputStream* output
) {
    google::protobuf::io::CodedOutputStream coded_output(output);
    return SerializeDelimitedToCodedStream(message, &coded_output);
}

bool SerializeDelimitedToCodedStream(
    google::protobuf::MessageLite const& message,
    google::protobuf::io::CodedOutputStream* output
) {
    // Write the size.
    size_t size = message.ByteSizeLong();
    if (size > INT_MAX) return false;

    output->WriteVarint32(size);

    // Write the content.
    google::protobuf::uint8* buffer = output->GetDirectBufferForNBytesAndAdvance(size);
    if (buffer != nullptr) {
        // Optimization: The message fits in one buffer, so use the faster
        // direct-to-array serialization path.
        message.SerializeWithCachedSizesToArray(buffer);
    } else {
        // Slightly-slower path when the message is multiple buffers.
        message.SerializeWithCachedSizes(output);
        if (output->HadError()) return false;
    }
    return true;
}

bool SerializeDelimitedToOstream(
    google::protobuf::MessageLite const& message,
    std::ostream* output
) {
    google::protobuf::io::OstreamOutputStream zero_copy_output(output);
    if (!SerializeDelimitedToZeroCopyStream(message, &zero_copy_output)) {
        return false;
    }
    return output->good();
}

bool GetDelimitedBodyFromCodedStream(
    google::protobuf::io::CodedInputStream* input,
    bool* clean_eof,
    std::string_view& out
) {
    if (clean_eof != nullptr) *clean_eof = false;
    int start = input->CurrentPosition();

    // Read the size.
    google::protobuf::uint32 size{};
    if (!input->ReadVarint32(&size)) {
        if (clean_eof != nullptr) *clean_eof = input->CurrentPosition() == start;
        return false;
    }

    // Get the position after any size bytes have been read (and only the message
    // itself remains).
    int position_after_size = input->CurrentPosition();

    // Tell the stream not to read beyond that size.
    google::protobuf::io::CodedInputStream::Limit limit = input->PushLimit(size);

    void const* data{};
    int sz{};
    if (! input->GetDirectBufferPointer(std::addressof(data), std::addressof(sz))) {
        return false;
    }
    out = {static_cast<char const*>(data), static_cast<std::size_t>(sz)};
    if (! input->Skip(sz)) {
        return false;
    }
//    if (!input->ConsumedEntireMessage()) return false;
    if (input->CurrentPosition() - position_after_size != static_cast<int>(size)) {
        return false;
    }

    // Release the limit.
    input->PopLimit(limit);

    return true;
}

bool GetDelimitedBodyFromZeroCopyStream(
    google::protobuf::io::ZeroCopyInputStream* input,
    bool* clean_eof,
    std::string_view& out
) {
    google::protobuf::io::CodedInputStream coded_input(input);
    return GetDelimitedBodyFromCodedStream(&coded_input, clean_eof, out);
}

bool PutDelimitedBodyToOstream(std::string_view in, std::ostream* output) {
    google::protobuf::io::OstreamOutputStream zero_copy_output(output);
    if (!PutDelimitedBodyToZeroCopyStream(in, &zero_copy_output)) {
        return false;
    }
    return output->good();

}

bool PutDelimitedBodyToZeroCopyStream(std::string_view in, google::protobuf::io::ZeroCopyOutputStream* output) {
    google::protobuf::io::CodedOutputStream coded_output(output);
    return PutDelimitedBodyToCodedStream(in, &coded_output);
}

bool PutDelimitedBodyToCodedStream(std::string_view in, google::protobuf::io::CodedOutputStream* output) {
    // Write the size.
    size_t size = in.size();
    if (size > INT_MAX) return false;

    output->WriteVarint32(size);

    // Write the content.
    output->WriteRaw(in.data(), size);
    return true;
}


}

