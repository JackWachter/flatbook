#include <benchmark/benchmark.h>
#include "events.hpp"
#include "orderbook.hpp"
#include "flatbook.hpp"

static const std::vector<Event>& events() {
    static const std::vector<Event> e = load_events("events.txt");
    return e;
}

static void BM_MapBook(benchmark::State& state) {
    const auto& ev = events();
    for (auto _ : state) {
        OrderBook book;
        benchmark::DoNotOptimize(apply_events(book, ev));
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(ev.size()));
}
BENCHMARK(BM_MapBook);

static void BM_FlatBook(benchmark::State& state) {
    const auto& ev = events();
    for (auto _ : state) {
        FlatOrderBook book;
        benchmark::DoNotOptimize(apply_events(book, ev));
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(ev.size()));
}
BENCHMARK(BM_FlatBook);

BENCHMARK_MAIN();