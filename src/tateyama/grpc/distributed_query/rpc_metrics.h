/* Copyright 2026 Project Tsurugi. Licensed under the Apache License, Version 2.0. */
#pragma once
#include <array>
#include <cstdint>
#include <tuple>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <tateyama/metrics/metrics_store.h>

namespace tateyama::grpc::distributed_query {
class rpc_metrics {
public:
    struct counters {
        std::atomic_uint64_t requests{};
        std::atomic_uint64_t active{};
        std::atomic_uint64_t elapsed_us{};
        std::array<std::atomic_uint64_t, 17> errors{};
    };
    explicit rpc_metrics(tateyama::metrics::metrics_store* store, std::string const& rpc)
        : counters_(std::make_shared<counters>()) {
        if (!store) return;
        auto add = [&](std::string const& key, auto reader, std::vector<std::tuple<std::string, std::string>> attributes) {
            store->register_item(tateyama::metrics::metrics_metadata{key, key, std::move(attributes), {}}, std::move(reader));
        };
        auto stats = counters_;
        add("distributed_query_requests_total", [stats] { return static_cast<double>(stats->requests.load(std::memory_order_relaxed)); }, {{"rpc", rpc}});
        add("distributed_query_requests_active", [stats] { return static_cast<double>(stats->active.load(std::memory_order_relaxed)); }, {{"rpc", rpc}});
        add("distributed_query_elapsed_microseconds_total", [stats] { return static_cast<double>(stats->elapsed_us.load(std::memory_order_relaxed)); }, {{"rpc", rpc}});
        for (std::size_t i = 1; i < stats->errors.size(); ++i) {
            add("distributed_query_errors_total", [stats, i] { return static_cast<double>(stats->errors.at(i).load(std::memory_order_relaxed)); }, {{"rpc", rpc}, {"status", std::to_string(i)}});
        }
    }
    class call {
    public:
        explicit call(rpc_metrics& owner) : counters_(owner.counters_), started_(std::chrono::steady_clock::now()) {
            counters_->requests.fetch_add(1, std::memory_order_relaxed);
            counters_->active.fetch_add(1, std::memory_order_relaxed);
        }
        call(call const&) = delete;
        call& operator=(call const&) = delete;
        call(call&&) = delete;
        call& operator=(call&&) = delete;
        ~call() { finish(2); } // unhandled exception = UNKNOWN; never record it as success
        void finish(int status) noexcept {
            if (finished_) return;
            finished_ = true;
            if (status != 0) {
                auto index = status > 0 && status < static_cast<int>(counters_->errors.size()) ? static_cast<std::size_t>(status) : std::size_t{2};
                counters_->errors.at(index).fetch_add(1, std::memory_order_relaxed);
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started_).count();
            counters_->elapsed_us.fetch_add(static_cast<std::uint64_t>(elapsed), std::memory_order_relaxed);
            counters_->active.fetch_sub(1, std::memory_order_relaxed);
        }
    private:
        std::shared_ptr<counters> counters_;
        std::chrono::steady_clock::time_point started_;
        bool finished_{};
    };
private:
    std::shared_ptr<counters> counters_;
};
}
