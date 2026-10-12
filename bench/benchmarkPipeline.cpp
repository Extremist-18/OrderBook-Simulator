// bench/pipeline_bench.cpp  (fixed version)
//
// Build (project root):
//   g++ -O3 -march=native -std=c++17 -pthread -Isrc bench/pipeline_bench.cpp src/core/OrderBook.cpp -o bench/pipeline_bench
// Run (project root, needs >= 3 free cores; do NOT use run_pinned.sh):
//   ./bench/pipeline_bench [N=1000000]
//
// What changed vs. the old test (see comments marked [FIX]).
#include "core/MatchingPipeline.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <thread>

using Clock = std::chrono::steady_clock;
static uint64_t nowNs(){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

// [FIX 1] Find out how coarse the clock really is. If this prints >= 1000 ns,
// every latency below that is rounded to 0 -- which is why p50 showed "0".
static uint64_t clockTickNs(){
    uint64_t best = ~0ull;
    for(int i = 0; i < 20000; i++){
        uint64_t a = nowNs(), b;
        do { b = nowNs(); } while(b == a);
        best = std::min(best, b - a);
    }
    return best;
}

struct Gen{
    std::mt19937_64 rng{42};
    Input next(uint64_t ticket){
        Input c{};
        c.cmd    = CmdType::NewOrder;
        c.ticket = ticket;
        c.side   = (rng() & 1) ? Side::Buy : Side::Sell;
        c.qnty   = 1 + (rng() % 50);
        if(rng() % 3 == 0){ c.type = OrderType::Market; c.price = 0; }
        else{ c.type = OrderType::Limit; c.price = 6'000'000 + (Price)(rng() % 200) - 100; }
        return c;
    }
};

static DepthUpdate makeDepth(){
    DepthUpdate d;
    d.nBids = d.nAsks = DEPTH;
    for(int i = 0; i < DEPTH; ++i){
        d.bids[i] = {6'000'000 - 1 - i, 1'000'000};
        d.asks[i] = {6'000'000 + 1 + i, 1'000'000};
    }
    return d;
}

// [FIX 2] Order-sensitive fingerprint of every trade (price, qty, side), not just
// totals. Two engines that trade differently can still have equal totals.
// (Buyer/seller ids are left out on purpose: OrderBook::setMarketDepth uses a
//  function-level `static OrderId marketId`, shared by ALL OrderBook instances,
//  so the reference book and the pipeline book get different depth-order ids.)
static inline void mix(uint64_t& h, uint64_t x){ h ^= x; h *= 1099511628211ull; }

struct Result{ bool ok; double ops, mean, p50, p90, p99, p999, max; };

int main(int argc, char** argv){
    const uint64_t N = argc > 1 ? strtoull(argv[1], nullptr, 10) : 1000000;
    std::ofstream csv("bench/pipeline_results.csv");
    csv << "run,target_ops,orders_per_sec,mean_us,p50_us,p90_us,p99_us,p999_us,max_us,correct\n";

    printf("clock tick ~ %llu ns | cores: %u | N = %llu\n",
           (unsigned long long)clockTickNs(), std::thread::hardware_concurrency(), (unsigned long long)N);

    // ───────── 1. baseline: single thread, no queues ─────────
    OrderBook ref;
    uint64_t refTrades = 0, refQty = 0, refHash = 1469598103934665603ull;
    {
        DepthUpdate d = makeDepth();
        std::vector<std::pair<Price,Quantity>> b, a;
        for(int i = 0; i < d.nBids; ++i) b.push_back({d.bids[i].price, d.bids[i].qnty});
        for(int i = 0; i < d.nAsks; ++i) a.push_back({d.asks[i].price, d.asks[i].qnty});
        ref.setMarketDepth(b, a);

        Gen g;  std::vector<Trade> tr;
        auto t0 = nowNs();
        for(uint64_t i = 1; i <= N; ++i){
            Input c = g.next(i);
            ref.addOrder(ref.nextId(), c.price, c.qnty, c.side, c.type);
            tr.clear(); ref.matchInto(tr);
            refTrades += tr.size();
            for(auto& t : tr){ refQty += t.quantity; mix(refHash, t.price); mix(refHash, t.quantity); mix(refHash, (uint64_t)t.aggressiveSide); }
        }
        double ops = N * 1e9 / (nowNs() - t0);
        printf("%-26s %10.0f orders/s   (engine only: no queues, no threads)\n", "inline baseline", ops);
        csv << "inline,," << ops << ",,,,,,,1\n";
    }

    // ───────── 2. the pipeline, at a given arrival rate (0 = flood) ─────────
    auto runPipeline = [&](const char* label, double rate) -> Result {
        OrderBook book;
        std::vector<uint64_t> submitNs(N + 2, 0);
        std::vector<double> lat;  lat.reserve(N);
        std::atomic<uint64_t> trades{0}, qty{0}, dones{0};
        std::atomic<bool> sawDepth{false};
        bool inOrder = true;
        uint64_t lastTicket = 0, hash = 1469598103934665603ull;     // publisher thread only
        const uint64_t warm = N / 10;                                // [FIX 3] drop cold-start samples

        MatchingPipeline pipe(book, [&](const std::vector<Event>& evs, const BookSnapshot* s){
            uint64_t now = nowNs();
            for(const Event& e : evs){
                if(e.type == Type::Trade){
                    trades++; qty += e.trade.quantity;
                    mix(hash, e.trade.price); mix(hash, e.trade.quantity); mix(hash, (uint64_t)e.trade.aggressiveSide);
                }else{
                    if(e.ticket != lastTicket + 1) inOrder = false;
                    lastTicket = e.ticket;
                    if(e.ticket > warm) lat.push_back((double)(now - submitNs[e.ticket]));
                    dones++;
                }
            }
            if(s && s->nBids >= SNAP) sawDepth = true;
        });
        pipe.start();

        DepthUpdate d = makeDepth();
        while(!pipe.submitDepth(d)) std::this_thread::yield();
        while(!sawDepth) std::this_thread::sleep_for(std::chrono::milliseconds(1));

        const double paceNs = rate > 0 ? 1e9 / rate : 0;
        Gen g;
        auto t0 = nowNs();
        for(uint64_t i = 1; i <= N; ++i){
            Input c = g.next(i);
            c.id = book.nextId();
            if(paceNs){
                // [FIX 4] Open-loop: stamp the INTENDED send time. If the producer stalls and
                // falls behind schedule, that delay counts as latency instead of being hidden
                // (coordinated omission).
                uint64_t intended = t0 + (uint64_t)(i * paceNs);
                while(nowNs() < intended) cpu_relax();
                submitNs[i] = intended;
            }else submitNs[i] = nowNs();
            while(!pipe.submit(c)) std::this_thread::yield();
        }
        while(dones < N) std::this_thread::sleep_for(std::chrono::microseconds(100));
        auto t1 = nowNs();
        pipe.stop();

        std::sort(lat.begin(), lat.end());
        auto pct = [&](double p){ return lat[(size_t)(p * (lat.size() - 1))] / 1000.0; };
        double sum = 0; for(double x : lat) sum += x;
        Result r{};
        r.ok = inOrder && refTrades == trades && refQty == qty && refHash == hash && ref.mid_price() == book.mid_price();
        r.ops = N * 1e9 / (t1 - t0);
        r.mean = sum / lat.size() / 1000.0;
        r.p50 = pct(.50); r.p90 = pct(.90); r.p99 = pct(.99); r.p999 = pct(.999); r.max = lat.back() / 1000.0;

        // [FIX 5] A flooded run measures THROUGHPUT only. The producer outruns the matcher, so
        // the 16384-slot input queue stays full and every order waits ~16384 / throughput
        // (~3 ms at 5.3M/s). That is queueing delay, not engine latency.
        if(rate == 0)
            printf("%-26s %10.0f orders/s | latency not meaningful when flooded (queue full) | %s\n",
                   label, r.ops, r.ok ? "results match" : "RESULTS DIFFER");
        else
            printf("%-26s %10.0f orders/s | mean %7.2f  p50 %7.2f  p90 %7.2f  p99 %7.2f  p99.9 %8.2f  max %8.1f us | %s\n",
                   label, r.ops, r.mean, r.p50, r.p90, r.p99, r.p999, r.max, r.ok ? "results match" : "RESULTS DIFFER");
        csv << label << "," << rate << "," << r.ops << "," << r.mean << "," << r.p50 << "," << r.p90 << ","
            << r.p99 << "," << r.p999 << "," << r.max << "," << r.ok << "\n";
        return r;
    };

    // [FIX 6] Latency is reported at several loads BELOW saturation (a load sweep), because that
    // is the curve people look for: latency stays flat, then climbs near max throughput.
    Result flood = runPipeline("flooded (max throughput)", 0);
    bool ok = flood.ok;
    const double fr[] = {0.25, 0.50, 0.75, 0.90};
    for(double f : fr){
        char label[64]; snprintf(label, sizeof label, "paced %2.0f%% (%.0fk/s)", f * 100, flood.ops * f / 1000);
        ok &= runPipeline(label, flood.ops * f).ok;
    }
    ok &= runPipeline("paced 100k/s", 100000).ok;

    printf("\n%s\nResults written to bench/pipeline_results.csv\n", ok ? "PASS: pipeline == single-threaded result" : "FAIL");
    return ok ? 0 : 1;
}