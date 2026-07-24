#include "events.hpp"
#include "orderbook.hpp"
#include "flatbook.hpp"

#include <gtest/gtest.h>

TEST(Generator, ProducesValidStream) {
    auto events = load_events("events.txt");
    OrderBook book;
    EXPECT_EQ(apply_events(book, events), 0);
}

TEST(Differential, BooksAgreeOnEveryEvent) {
    auto events = load_events("events.txt");
    ASSERT_FALSE(events.empty()) << "events.txt was empty";

    OrderBook     map_book;
    FlatOrderBook flat_book;

    for (size_t i = 0; i < events.size(); ++i) {
        const Event& e = events[i];
        int a = 0, b = 0;

        switch (e.action) {
            case Action::Add:
                a = map_book.add(e.id, e.side, e.price, e.quantity);
                b = flat_book.add(e.id, e.side, e.price, e.quantity);
                break;
            case Action::Cancel:
                a = map_book.cancel(e.id);
                b = flat_book.cancel(e.id);
                break;
            case Action::Execute:
                a = map_book.execute(e.id, e.quantity);
                b = flat_book.execute(e.id, e.quantity);
                break;
        }
        ASSERT_EQ(a, b) << "return codes differ at event " << i;

        Quote mb = map_book.best_bid(), fb = flat_book.best_bid();
        Quote ma = map_book.best_ask(), fa = flat_book.best_ask();

        ASSERT_EQ(mb.price, fb.price)    << "best_bid price diverged at event "  << i;
        ASSERT_EQ(mb.quantity, fb.quantity) << "best_bid volume diverged at event " << i;
        ASSERT_EQ(ma.price, fa.price)    << "best_ask price diverged at event "  << i;
        ASSERT_EQ(ma.quantity, fa.quantity) << "best_ask volume diverged at event " << i;
    }
}

TEST(BestQuotes, ReplayValidatesInvariants) {
    auto events = load_events("events.txt");
    OrderBook book;
    apply_events(book, events);

    ASSERT_GT(book.best_bid().price, -1);
    ASSERT_GT(book.best_ask().price, -1);
}

TEST(Instrumentation, SlideCounts) {
    auto events = load_events("events.txt");
    FlatOrderBook book;
    apply_events(book, events);
    std::cout << "slides up: "   << book.slide_up_count()
              << "  down: "      << book.slide_down_count()
              << "  migrated: "  << book.migration_count() << "\n";
}