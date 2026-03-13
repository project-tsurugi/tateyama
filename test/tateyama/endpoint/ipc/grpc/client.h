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
#pragma once

#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#include <gflags/gflags.h>

#include <data_relay_grpc/blob_relay/api_version.h>
#include <data_relay_grpc/proto/blob_relay/blob_relay_streaming.grpc.pb.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

namespace tateyama::endpoint::ipc::grpc {

using namespace std::literals::string_literals;
static std::string reference_data = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n"s;

class Client {
    constexpr static std::size_t buffer_size = 32;

    class test_blob {
    public:
        test_blob() {
        }
        bool compare(const std::string& data, std::size_t s) {
            if (s < 1) {
                return false;
            }

            std::size_t size = reference_data.size();
            bool rv = false;
            std::size_t e = p_ + s;
            if ((p_ / size) == ((e - 1) / size)) {
                rv = reference_data.substr(p_ % size, s).compare(data.substr(0, s)) == 0;
            } else {
                std::size_t b = p_ % size;
                std::size_t rem = size - b;

                if (reference_data.substr(b, rem).compare(data.substr(0, rem)) == 0) {
                    rv = reference_data.substr(0, s - rem).compare(data.substr(rem)) == 0;
                }
            }
            p_ += s;
            return rv;
        }
        std::string& chunk() {
            return reference_data;
        }
        std::size_t length() {
            return reference_data.length();
        }
    private:
        std::size_t p_{0};
    };

public:
    Client(const std::string& server_address, std::size_t session_id) : server_address_(server_address), session_id_(session_id) {
    }

    void put(std::size_t size, std::uint64_t& object_id, std::uint64_t& tag) {
        auto channel = CreateChannel(server_address_);
        data_relay_grpc::proto::blob_relay::blob_relay_streaming::BlobRelayStreaming::Stub stub(channel);
        ::grpc::ClientContext context;

        std::unique_ptr<::grpc::ClientWriter<data_relay_grpc::proto::blob_relay::blob_relay_streaming::PutStreamingRequest> > writer(stub.Put(&context, &res_));

        // send metadata
        data_relay_grpc::proto::blob_relay::blob_relay_streaming::PutStreamingRequest req_metadata;
        auto* metadata = req_metadata.mutable_metadata();
        metadata->set_api_version(BLOB_RELAY_API_VERSION);
        metadata->set_session_id(session_id_);
        metadata->set_blob_size(size);
        if (!writer->Write(req_metadata)) {
            throw std::runtime_error(std::string("error in ") + __func__ + " at " + std::to_string(__LINE__));
        }

        // send blob data begin
        data_relay_grpc::proto::blob_relay::blob_relay_streaming::PutStreamingRequest req_chunk;
        std::size_t transfered_size{};
        do {
            test_blob reference{};
            std::size_t chunk_size = std::min(reference.length(), size - transfered_size);
            req_chunk.set_chunk(reference.chunk().data(), chunk_size);
            transfered_size += chunk_size;
            if (!writer->Write(req_chunk)) {
                throw std::runtime_error(std::string("error in ") + __func__ + " at " + std::to_string(__LINE__));
            }
        } while (transfered_size < size);
        writer->WritesDone();
        ::grpc::Status status = writer->Finish();
        if (status.error_code() != ::grpc::StatusCode::OK) {
            throw std::runtime_error(std::string("error in ") + __func__ + " at " + std::to_string(__LINE__) + ", message = `" + status.error_message () + "'");
        }
        object_id = res_.blob().object_id();
        tag = res_.blob().tag();
    }

    void get(std::uint64_t blob_id, std::uint64_t tag, std::filesystem::path path) {
        auto channel = CreateChannel(server_address_);
        data_relay_grpc::proto::blob_relay::blob_relay_streaming::BlobRelayStreaming::Stub stub(channel);
        ::grpc::ClientContext context;
        data_relay_grpc::proto::blob_relay::blob_relay_streaming::GetStreamingRequest req;
        req.set_api_version(BLOB_RELAY_API_VERSION);
        req.set_session_id(session_id_);
        auto* blob = req.mutable_blob();
        blob->set_object_id(blob_id);
        blob->set_tag(tag);

        std::unique_ptr<::grpc::ClientReader<data_relay_grpc::proto::blob_relay::blob_relay_streaming::GetStreamingResponse> > reader(stub.Get(&context, req));

        data_relay_grpc::proto::blob_relay::blob_relay_streaming::GetStreamingResponse resp;
        std::ofstream blob_file(path);
        if (!blob_file) {
            throw std::runtime_error(std::string("error in ") + __func__ + " at " + std::to_string(__LINE__));
        }
        if (reader->Read(&resp)) {
            if (resp.payload_case() != data_relay_grpc::proto::blob_relay::blob_relay_streaming::GetStreamingResponse::PayloadCase::kMetadata) {
                throw std::runtime_error("first response is not a metadata");
            }
            std::size_t blob_size = resp.metadata().blob_size();  // streaming_service always set blob_size

            while (reader->Read(&resp)) {
                auto& chunk = resp.chunk();
                blob_file.write(chunk.data(), chunk.length());
            }
            blob_file.close();
            if (std::filesystem::file_size(path) != blob_size) {
                throw std::runtime_error("inconsistent blob size");
            }
        }
        ::grpc::Status status = reader->Finish();
        if (status.error_code() != ::grpc::StatusCode::OK) {
            throw std::runtime_error(std::string("error in ") + __func__ + " at " + std::to_string(__LINE__) + ", message = `" + status.error_message () + "'");
        }
    }

    static bool compare(std::filesystem::path const path) {
        std::ifstream blob_file(path);
        if (!blob_file) {
            return false;
        }

        blob_file.seekg(0, std::ios::end);
        std::streamsize fileSize = blob_file.tellg();
        blob_file.seekg(0, std::ios::beg);

        std::string buffer{};
        buffer.resize(buffer_size);

        if (fileSize == 0) {
            std::stringstream ss{};
            ss << "file size of " << path.string() << " is " << std::filesystem::file_size(path) << " bytes";
            std::cout << ss.str() << std::endl;
            return true;
        }

        test_blob reference{};
        while (blob_file.tellg() < fileSize) {
            auto size = std::min(static_cast<std::size_t>(fileSize - blob_file.tellg()), buffer_size);
            blob_file.read(reinterpret_cast<char*>(buffer.data()), size);
            if (!reference.compare(buffer, size)) {
                return false;
            }
        }
        {
            std::stringstream ss{};
            ss << "file size of " << path.string() << " is " << std::filesystem::file_size(path) << " bytes, and that is the same as the reference data";
            std::cout << ss.str() << std::endl;
        }
        return true;
    }

private:
    std::string server_address_;
    std::size_t session_id_;
    std::unique_ptr<data_relay_grpc::proto::blob_relay::blob_relay_streaming::BlobRelayStreaming::Stub> stub_;
    data_relay_grpc::proto::blob_relay::blob_relay_streaming::PutStreamingResponse res_;

    static std::shared_ptr< ::grpc::Channel > CreateChannel(const std::string& server_address) {
        return ::grpc::CreateChannel(server_address, ::grpc::InsecureChannelCredentials());
    }
};

}  // namespace
