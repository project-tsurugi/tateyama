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
    google::protobuf::io::CodedInputStream::Limit limit = input->PushLimit(static_cast<int>(size));

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
    google::protobuf::uint8* buffer = output->GetDirectBufferForNBytesAndAdvance(static_cast<int>(size));
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
    google::protobuf::io::CodedInputStream::Limit limit = input->PushLimit(static_cast<int>(size));

    void const* data{};
    int dsz{};
    if (! input->GetDirectBufferPointer(std::addressof(data), std::addressof(dsz))) {
        return false;
    }
    out = {static_cast<char const*>(data), static_cast<std::size_t>(dsz)};
    if (! input->Skip(dsz)) {
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

bool PutDelimitedBodyToOstream(std::string_view input, std::ostream* output) {
    google::protobuf::io::OstreamOutputStream zero_copy_output(output);
    if (!PutDelimitedBodyToZeroCopyStream(input, &zero_copy_output)) {
        return false;
    }
    return output->good();

}

bool PutDelimitedBodyToZeroCopyStream(std::string_view input, google::protobuf::io::ZeroCopyOutputStream* output) {
    google::protobuf::io::CodedOutputStream coded_output(output);
    return PutDelimitedBodyToCodedStream(input, &coded_output);
}

bool PutDelimitedBodyToCodedStream(std::string_view input, google::protobuf::io::CodedOutputStream* output) {
    // Write the size.
    size_t size = input.size();
    if (size > INT_MAX) return false;

    output->WriteVarint32(size);

    // Write the content.
    output->WriteRaw(input.data(), static_cast<std::int32_t>(size));
    return true;
}


}

