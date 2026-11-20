/* Copyright 2026 Project Tsurugi. Licensed under the Apache License, Version 2.0. */
#pragma once
#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <tateyama/grpc/distributed_query/service_adapter.h>
namespace tateyama::grpc::distributed_query {
inline std::string observation_id(std::string_view input) {
    std::string out;
    for (unsigned char c : input.substr(0, 128)) out += c >= 0x21 && c <= 0x7e ? static_cast<char>(c) : '_';
    return out.empty() ? "none" : out;
}
inline std::string worker_observation(remote_process_observation const& observed) {
    std::ostringstream out;
    auto field = [&](char const* name, auto const& value) {
        out << ' ' << name << ':';
        if (value) out << *value; else out << "unobserved";
    };
    out << " worker_phase:" << observation_id(observed.phase);
    field("worker_job_id", observed.local_job_id);
    field("rtx_handle", observed.transaction_handle);
    out << " rtx_id:" << observation_id(observed.transaction_id);
    field("rtx_begin_status", observed.begin_status);
    field("rtx_commit_status", observed.commit_status);
    out << " rtx_rollback_requested:" << observed.rollback_requested;
    field("rtx_rollback_status", observed.rollback_status);
    field("rtx_close_status", observed.close_status);
    return out.str();
}
}
