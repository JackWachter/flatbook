# Flatbook
A C++20 limit order book using a flat, direct-indexed price window, benchmarked against a `std::map`-based implementation. This project is meant to be an experimental measurement as to where the array beats the tree and where it does not, how the structures compare as book density varies, what the flatbook's failure mode is, and what causes the speed difference at a hardware level.

## Design

Both books expose the same five operations: add, cancel, execute, best_bid, best_ask. They differ in how a price maps to a price level.

**Baseline (OrderBook)**. Two `std::map<Price, Level>` (bids/asks), each level a FIFO list of orders plus a volume total, with an `unordered_map<OrderId, Location>` for O(1) cancel. Best bid/ask are the ends of the tree. Price is an integer tick count, never a float, so index math is exact.

**Flatbook (FlatOrderBook)**. A fixed-size array (`WINDOW_SIZE = 64`) of price levels per side, indexed directly by `price - base`. Key pieces:
- **Sliding window.** The window tracks the active price range. When an add lands within a chunk of the edge, the window slides by `CHUNK` and the base moves with it, a rotating `head` makes the slide O(1) for surviving orders (they aren't moved, only the base/head advance).
- **Spillover.** Prices outside the window go into a `std::map`, so no order is ever lost and the flat book stays a faithful reconstruction. Slides migrate orders between the array and spillover in both directions.
- **O(1) touch.** A 64-bit occupancy bitmask per side; `std::rotr` by `head` converts physical slot order to logical price order, then `countl_zero`/`countr_zero` locates the inside level in a couple of instructions.
- **Intrusive node pool.** Orders live in one preallocated pool threaded by index, not per-level heap-allocated list nodes. This removes an allocation per order and turns slide migration into pure pointer relinking.

## Performance

At the flat book's design density (~50 active price levels):

| Implementation  | Throughput     | vs baseline |
| --------------- | -------------: | ----------: |
| `std::map` book | 15.4M events/s | 1.0×        |
| flatbook        | 39.9M events/s | **2.6×**    |

Results will vary by machine.

### What drives the speedup

`perf stat` each book replayed in isolation:

| Metric            | `std::map` | flatbook |
| ----------------- | ---------: | -------: |
| cache-miss rate   | 0.18%      | 3.94%    |
| instructions      | 5.71B      | 3.60B    |
| instructions/cycle| 2.16       | 3.54     |

At this book size the map's tree is small enough to sit in cache (0.18% miss rate), so the flat book does not win on cache. The overall win comes mainly from executing ~37% fewer instructions, found in arithmetic indexing vs tree descent, and retiring them at higher IPC.

The cache advantage only appears once the working set exceeds cache. With a wide book (hundreds of thousands of resting orders), the map's miss rate climbs to 36.5% and its IPC falls to 0.34, pointer-chasing becomes memory stall. At that width the flat book is past its window and running mostly through its spillover map, so this regime measures the map's cache dropoff.

In conclusion: the map can compete in small books due to its cache residency. But, the flat book wins at its design density on instruction count and pipeline efficiency, and the tree's disadvantage grows as it leaves cache.

### Latency distribution

Per-operation latency, flat book:
 
| Percentile | Latency  |
| ---------- | -------: |
| p50        | 40 ns    |
| p99        | 70 ns    |
| p99.9      | 1223 ns  |
| max        | 12764 ns |
Note: Includes ~20–30ns clock-read overhead, which dominates the median.

The common path is fast and flat and the tail is the window slide. One add in a thousand triggers a slide that migrates a batch of orders, which is ~30× the median at p99.9 and worse at the max. A mean would hide this entirely.

## The slide-thrashing finding
 
This was the main bug that stood out to me while developing. With the window at 64 slots and a chunk of 16, a trending stream that pushed the active range to ~61 levels collapsed the flat book to 0.27× the baseline which is a 6.7× slowdown.
 
Instrumenting the slide path helped me understand what happened. With only ~3 ticks of slack between the active range and the window, a 16-tick slide overshoots past the opposite boundary, forcing an immediate reverse slide causing the window to oscillate.
 
| Config (±30 stream) | slides | order migrations |
| ------------------- | -----: | ---------------: |
| `CHUNK = 16`        | 474    | 126,795          |
| `CHUNK = 4`         | 22     | 1,207            |
 
Shrinking the chunk produced ~20× fewer slides and ~100× fewer migrations, and recovered throughput to ~30M events/s. Smaller chunks slide less, because a small step accommodates the price without overshooting. The window needs at least a chunk of slack beyond the active spread. `CHUNK = 4` cost nothing at narrower densities, so it is uniformly better here.

## Build

Requires CMake 3.20+ and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release

cmake --build build
cmake --build build-release
```

## Run tests

```bash
./build/tests
```

## Run benchmarks

Generate the input events:

```bash
c++ -std=c++20 -O2 tools/generator.cpp -o generator
./generator
```

Run the throughput benchmark:

```bash
taskset -c 2 ./build-release/benchmarks \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
```

Run the latency benchmark:

```bash
taskset -c 2 ./build-release/latency
```