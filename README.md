A C++20 limit order book using a flat, direct-indexed price window, benchmarked against a `std::map`-based implementation.

## Performance

**FlatBook is approximately 2.4× faster than the `std::map`-based book** in this benchmark.

| Implementation  |  Mean time |     Throughput |
| --------------- | ---------: | -------------: |
| `std::map` book | 645,350 ns | 15.5M events/s |
| FlatBook        | 267,435 ns | 37.4M events/s |

Results will vary by machine.

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