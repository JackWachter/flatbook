#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdio>
#include "events.hpp"
#include "flatbook.hpp"

int main() {
    auto events = load_events("events.txt");
    FlatOrderBook book;

    std::vector<double> samples;
    samples.reserve(events.size());

    for (const Event& e : events) {
        auto t0 = std::chrono::steady_clock::now();
        switch (e.action) {
            case Action::Add:     book.add(e.id, e.side, e.price, e.quantity); break;
            case Action::Cancel:  book.cancel(e.id); break;
            case Action::Execute: book.execute(e.id, e.quantity); break;
        }
        auto t1 = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }

    std::sort(samples.begin(), samples.end());
    auto pct = [&](double p) {
        return samples[static_cast<size_t>(p * (samples.size() - 1))];
    };
    std::printf("p50  %.1f ns\n", pct(0.50));
    std::printf("p99  %.1f ns\n", pct(0.99));
    std::printf("p99.9 %.1f ns\n", pct(0.999));
    std::printf("max  %.1f ns\n", samples.back());
    return 0;
}