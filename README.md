# OrderBook Simulator

A C++ limit order book with price-time priority matching, a synthetic market simulator, and a live ImGui/ImPlot dashboard for watching the book move in real time.

I built this to understand how exchanges actually manage orders under the hood.

---

## What it does

- Maintains a live limit order book (bids and asks) with proper price-time priority
- Matches limit and market orders against resting liquidity
- Runs a background simulator thread that generates realistic-ish order flow (a sentiment-driven random walk decides how aggressively buyers/sellers behave)
- Renders the whole thing live: current price, best bid/ask, spread, a price chart, and top-10 depth on both sides — via ImGui + ImPlot

It is **not** connected to a real exchange or real market data (yet — see [Roadmap](#roadmap)). Everything you see is synthetic order flow generated locally, which makes it a safe sandbox for experimenting with matching logic without worrying about real money or real data feeds.

---

## Demo

![OrderBook Simulator Demo Img](demo1.png)

![Demo Img](demo2.png)

---

## Architecture

The project is split into three layers 

```
┌─────────────────────────────────────────────--┐
│                  main.cpp                     │
│   GLFW window + ImGui/ImPlot rendering loop   │
└───────────────────┬───────────────────────────┘
                     │ reads via thread-safe getters
┌────────────────────▼───────────────────────────--┐
│              simulation/Simulator                │
│  Background thread.                               │
│   - generates buy/sell/limit orders               │
│   - feeds them into the OrderBook                 │
│   - calls book.match()                            │
└────────────────────┬──────────────────────────---─┘
                     │ addOrder() / cancelOrder() / match()
┌────────────────────▼──────────────────────────---─┐
│                 core/OrderBook                    │
│  bids: std::map<Price, PriceLevel, greater<>>     │
│  asks: std::map<Price, PriceLevel, less<>>        │
│  orderMetadata: unordered_map<OrderId, {side,price}>│
│  Guarded by a single mutex                          │
└──────────────────────────────────────────────────--┘
```

**Why `std::map` for price levels?** It keeps bids sorted highest-first and asks sorted lowest-first automatically, so `best_bid()`/`best_ask()` are O(1) (just `begin()`), and price-level lookups are O(log n). 

---

## Order structure

Every order is a fixed 32-byte struct, kept small to optimize cache line calls

```cpp
using OrderId   = uint64_t;
using Price     = int64_t;   // fixed-point, e.g. 12000 = $120.00 (2 implied decimals)
using Quantity  = uint32_t;

enum class Side: uint8_t      { Buy = 0, Sell = 1 };
enum class OrderType: uint8_t { Limit = 0, Market = 1 };

struct Order {
    OrderId   orderId;   // 8 bytes
    uint64_t  seqNum;    // 8 bytes - insertion order, used for time priority
    Price     price;     // 8 bytes
    Quantity  quantity;  // 4 bytes
    Side      side;      // 1 byte
    OrderType type;       // 1 byte
    // (2 bytes padding)
};

```

A few choices worth explaining:

- **Price is an integer, not a float.** Prices are stored as fixed-point integers (cents, or whatever precision you pick) instead of `double`. This avoids floating-point comparison bugs when checking whether two prices are equal — a classic source of subtle exchange-matching bugs.
- **`seqNum` gives time priority.** Orders at the same price level are matched in the order they arrived, which is what "price-time priority" actually means in practice. It's a monotonically increasing counter assigned at insertion.
- **Resting orders live inside a `PriceLevel`:**

```cpp
struct PriceLevel {
    std::vector<Order> orders;              // FIFO queue at this price
    size_t head = 0;                        // index of the oldest live order
    std::unordered_map<OrderId, size_t> idx_map; // orderId -> index, for O(1) cancel lookup
};
```

  Cancelling an order doesn't remove it from the vector immediately (that would be O(n) — expensive under load). Instead it's marked dead (`quantity = 0`) and skipped over during matching. This is a standard "lazy deletion" pattern in order book design.

---

## Getting started

### Dependencies

- CMake ≥ 3.15
- A C++17 compiler
- OpenGL
- GLFW3

On macOS:
```bash
brew install cmake glfw
```

On Ubuntu/Debian:
```bash
sudo apt install cmake libglfw3-dev libgl1-mesa-dev
```

`imgui` and `implot` are vendored as source (not submodules) under `imgui/` and `implot/` — they're gitignored in this repo, so you'll need to grab them yourself if you're cloning fresh:

```bash
git clone https://github.com/ocornut/imgui.git
git clone https://github.com/epezent/implot.git
```
(Drop both into the project root, next to `src/`.)

### Build

```bash
mkdir build && cd build
cmake ..
make -j
```

### Run

```bash
./orderbook_gui
```

This opens a window showing live price, best bid/ask, spread, a rolling price chart, and top-10 order book depth on both sides. The simulator starts automatically and generates order flow in the background — you don't need to do anything else to see it move.

---

## Known limitations / Roadmap

Being upfront about what's rough around the edges, roughly in priority order:

- [ ] **Memory growth in `PriceLevel`** — cancelling an order marks it dead but doesn't compact the underlying vector unless the whole price level empties out via matching. A price level that's added-to and cancelled-from repeatedly without ever crossing will grow unbounded. Fix: periodic compaction, or move to an intrusive doubly-linked list per price level.
- [ ] **Market order trade price** — currently market-order fills are stamped with an internal drifting `marketPrice` variable rather than the actual price of the resting order they matched against. Should use the resting order's price, which is how real price-time-priority matching works.
- [ ] **Matching is batched, not continuous** — orders are inserted first and reconciled by a separate `match()` call (currently every ~50ms from the simulator), rather than matching immediately against the opposite book at insertion time like a real exchange does.
- [ ] **Single global mutex** — correct, but coarse. Worth exploring a single-writer-thread + lock-free queue design if I want to push toward genuinely low-latency numbers.
- [ ] **No automated tests** — need a real suite (Catch2/GoogleTest) covering matching correctness, FIFO ordering, partial fills, and edge cases (empty book, self-cross, etc.), plus a stress test that asserts invariants under random order flow.
- [ ] **No latency/throughput benchmarks yet** — want a `bench/` harness reporting p50/p95/p99/p99.9 latency per operation, not just "it feels fast."
- [ ] **No real market data** — replacing the synthetic sentiment-driven simulator with a replay of real L2 tick data (e.g. from Binance or Coinbase) would make the whole thing much more realistic.
- [ ] Support for IOC/FOK/GTC order types, order modify, and iceberg orders.

---
