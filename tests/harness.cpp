#include <fstream>
#include <sstream>
#include <cstdint>
#include "orderbook.hpp"
#include <gtest/gtest.h>

int replay(OrderBook& book, const std::string& filename) {
    std::fstream in(filename);
    std::string line;
    int failures = 0;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        char action;
        iss >> action;
        if (action == 'A') {
            OrderId id; char side_char; Price price; Quantity qty;
            iss >> id >> side_char >> price >> qty;
            Side side = (side_char == 'B') ? Side::Buy : Side::Sell;
            book.add(id, side, price, qty);
        } else if (action == 'C') {
            OrderId id;
            iss >> id;
            if (book.cancel(id) != 0) failures++;
        } else if (action == 'E') {
            OrderId id; Quantity qty;
            iss >> id >> qty;
            if (book.execute(id, qty) != 0) failures++; 
        }
    }
    return failures;
}

TEST(BestQuotes, ReplayValidatesInvariants) {
    OrderBook book;
    replay(book, "events.txt");

    ASSERT_GT(book.best_bid().price, -1);
    ASSERT_GT(book.best_ask().price, -1);
    EXPECT_GE(book.best_bid().price, 95);
    EXPECT_LE(book.best_bid().price, 105);
    EXPECT_GE(book.best_ask().price, 95);
    EXPECT_LE(book.best_ask().price, 105);
}

TEST(Generator, ProducesValidStream) {
    OrderBook book;
    int failures = replay(book, "events.txt");
    EXPECT_EQ(failures, 0);
}
