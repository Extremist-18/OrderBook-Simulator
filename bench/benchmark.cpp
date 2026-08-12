// bench/benchmark.cpp
//
// Latency + throughput benchmark for OrderBook.
//
// Measures individual operations (addOrder, cancelOrder, match) with
// warmup + percentile reporting, then a separate sustained-throughput run.
// Results print to stdout AND get written to bench/results.csv so you can
// plot them (see bench/plot_results.py) or diff runs over time.
//
// Build:
//   g++ -O2 -std=c++17 -Isrc bench/benchmark.cpp src/core/OrderBook.cpp -o bench/benchmark
// Run:
//   ./bench/benchmark
//
// Notes on methodology (why it's built this way):
//   - Always compile benchmarks with -O2/-O3. A debug build's numbers are
//     meaningless -- you're benchmarking the compiler, not your design.
//   - Warm up before measuring. The first few thousand calls pay for cold
//     caches, page faults, and (if applicable) branch predictor training.
//     Discard them.
//   - Measure ONE call per sample, not a batch divided by N. Batching
//     hides jitter and lets the compiler/CPU amortize costs you'd actually
//     pay per-order in production.
//   - Pin the process to one core when you can (see run_pinned.sh) so the
//     OS scheduler doesn't migrate you mid-run and blow up your tail
//     latency for reasons that have nothing to do with your code.

#include "core/OrderBook.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

struct Stats {
    double mean, p50, p95, p99, p999, max;
};

static Stats summarize(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    auto pct = [&](double p) {
        size_t idx = static_cast<size_t>(p * (samples.size() - 1));
        return samples[idx];
    };
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    return Stats{
        sum / samples.size(),
        pct(0.50),
        pct(0.95),
        pct(0.99),
        pct(0.999),
        samples.back()
    };
}

static void print_and_log(std::ofstream& csv, const std::string& name, const Stats& s) {
    std::cout << name << ":\n"
              << "  mean=" << s.mean  << "ns  "
              << "p50="   << s.p50   << "ns  "
              << "p95="   << s.p95   << "ns  "
              << "p99="   << s.p99   << "ns  "
              << "p99.9=" << s.p999  << "ns  "
              << "max="   << s.max   << "ns\n\n";
    csv << name << "," << s.mean << "," << s.p50 << "," << s.p95 << ","
        << s.p99 << "," << s.p999 << "," << s.max << "\n";
}

template <typename F>
static std::vector<double> measure(int warmup, int n, F op) {
    for (int i = 0; i < warmup; i++) op(i);
    std::vector<double> samples;
    samples.reserve(n);
    for (int i = 0; i < n; i++) {
        auto t0 = Clock::now();
        op(warmup + i);
        auto t1 = Clock::now();
        samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    return samples;
}

int main() {
    constexpr int WARMUP = 5000;
    constexpr int N = 100000;

    std::mt19937_64 rng(42); // fixed seed -> reproducible runs, easier to diff
    std::uniform_int_distribution<int> price_dist(11000, 13000);
    std::uniform_int_distribution<int> qty_dist(1, 1000);

    std::ofstream csv("bench/results.csv");
    csv << "operation,mean_ns,p50_ns,p95_ns,p99_ns,p999_ns,max_ns\n";

    std::cout << "=== OrderBook Benchmark ===\n";
    std::cout << "warmup=" << WARMUP << " samples=" << N << "\n\n";

    // 1. addOrder: worst case -- price is random, so this frequently
    //    creates a brand-new PriceLevel (map insertion) rather than
    //    appending to an existing one.
    {
        OrderBook book;
        OrderId id = 0;
        auto samples = measure(WARMUP, N, [&](int i) {
            Price p = price_dist(rng);
            Quantity q = qty_dist(rng);
            Side s = (i % 2 == 0) ? Side::Buy : Side::Sell;
            book.addOrder(id++, p, q, s, OrderType::Limit);
        });
        print_and_log(csv, "addOrder_random_price", summarize(samples));
    }

    // 2. addOrder: best case -- same price level every time, so this is
    //    purely a vector push_back + hash map insert, no tree rebalancing.
    {
        OrderBook book;
        OrderId id = 0;
        auto samples = measure(WARMUP, N, [&](int i) {
            Quantity q = qty_dist(rng);
            Side s = (i % 2 == 0) ? Side::Buy : Side::Sell;
            book.addOrder(id++, 12000, q, s, OrderType::Limit);
        });
        print_and_log(csv, "addOrder_same_price_level", summarize(samples));
    }

    // 3. cancelOrder: pre-load N+warmup orders, then cancel each one once.
    {
        OrderBook book;
        OrderId id = 0;
        std::vector<OrderId> ids;
        ids.reserve(N + WARMUP);
        for (int i = 0; i < N + WARMUP; i++) {
            Price p = price_dist(rng);
            Quantity q = qty_dist(rng);
            Side s = (i % 2 == 0) ? Side::Buy : Side::Sell;
            book.addOrder(id, p, q, s, OrderType::Limit);
            ids.push_back(id++);
        }
        int i = 0;
        auto samples = measure(WARMUP, N, [&](int) { book.cancelOrder(ids[i++]); });
        print_and_log(csv, "cancelOrder", summarize(samples));
    }

    // 4. match(): pre-load resting liquidity on both sides, then repeatedly
    //    feed one crossing pair + call match(), so every call does real work.
    {
        OrderBook book;
        OrderId id = 0;
        for (int i = 0; i < 2000; i++) {
            book.addOrder(id++, 11000 + (i % 500), qty_dist(rng), Side::Buy, OrderType::Limit);
            book.addOrder(id++, 13000 - (i % 500), qty_dist(rng), Side::Sell, OrderType::Limit);
        }
        auto samples = measure(1000, 20000, [&](int) {
            book.addOrder(id++, 12500, qty_dist(rng), Side::Buy, OrderType::Limit);
            book.addOrder(id++, 11500, qty_dist(rng), Side::Sell, OrderType::Limit);
            book.match();
        });
        print_and_log(csv, "addOrder_x2_plus_match_crossing", summarize(samples));
    }

    // 5. Sustained throughput: how many orders/sec can the engine absorb
    //    end-to-end (adds + periodic matching), not per-op latency.
    {
        OrderBook book;
        OrderId id = 0;
        constexpr int TOTAL = 500000;
        auto t0 = Clock::now();
        for (int i = 0; i < TOTAL; i++) {
            Price p = price_dist(rng);
            Quantity q = qty_dist(rng);
            Side s = (i % 2 == 0) ? Side::Buy : Side::Sell;
            book.addOrder(id++, p, q, s, OrderType::Limit);
            if (i % 10 == 0) book.match();
        }
        auto t1 = Clock::now();
        double secs = std::chrono::duration<double>(t1 - t0).count();
        double throughput = TOTAL / secs;
        std::cout << "Sustained throughput: " << throughput << " orders/sec ("
                  << TOTAL << " orders in " << secs << "s)\n\n";
        csv << "throughput_orders_per_sec,,,,,," << throughput << "\n";
    }

    std::cout << "Results written to bench/results.csv\n";
    return 0;
}
