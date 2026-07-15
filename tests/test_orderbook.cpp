#include "orderbook.hpp"

#include <gtest/gtest.h>

TEST(Skeleton, Compiles) {
    OrderBook book;
    (void)book;
}

TEST(Add, CompilesAndRuns) {
    OrderBook book;
    int result = book.add(1, Side::Buy, 100, 50);
    EXPECT_EQ(result, 0);
}

TEST(BestQuotes, ReadsBackCorrectly) {
    OrderBook book;
    book.add(1, Side::Buy, 100, 50);
    book.add(2, Side::Buy, 101, 30);
    book.add(3, Side::Sell, 105, 40);
    book.add(4, Side::Sell, 104, 20);

    EXPECT_EQ(book.best_bid().price, 101);
    EXPECT_EQ(book.best_bid().quantity, 30);
    EXPECT_EQ(book.best_ask().price, 104);
    EXPECT_EQ(book.best_ask().quantity, 20);
}

TEST(BestQuotes, EmptyBookSentinel) {
    OrderBook book;
    EXPECT_EQ(book.best_bid().price, -1);
    EXPECT_EQ(book.best_ask().price, -1);
}

TEST(Cancel, RemovesOrderAndUpdatesVolume) {
    OrderBook book;
    book.add(1, Side::Buy, 100, 50);
    book.add(2, Side::Buy, 100, 30);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.best_bid().price, 100);
    EXPECT_EQ(book.best_bid().quantity, 30);
}

TEST(Cancel, EmptyingLevelMovesTouch) {
    OrderBook book;
    book.add(1, Side::Buy, 101, 50);
    book.add(2, Side::Buy, 100, 30);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.best_bid().price, 100);
}

TEST(Cancel, UnknownIdReturnsError) {
    OrderBook book;
    EXPECT_EQ(book.cancel(999), 1);
}

TEST(Cancel, LastOrderLeavesEmptyBook) {
    OrderBook book;
    book.add(1, Side::Buy, 100, 50);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.best_bid().price, -1);
}

TEST(Execute, PartialFillReducesAndKeepsOrder) {
    OrderBook book;
    book.add(1, Side::Buy, 100, 100);
    EXPECT_EQ(book.execute(1, 30), 0);
    EXPECT_EQ(book.best_bid().quantity, 70);
}

TEST(Execute, FullFillRemovesOrder) {
    OrderBook book;
    book.add(1, Side::Buy, 100, 50);
    book.add(2, Side::Buy, 100, 20);
    EXPECT_EQ(book.execute(1, 50), 0);
    EXPECT_EQ(book.best_bid().quantity, 20);
}

TEST(Execute, FullFillEmptyingLevelMovesTouch) {
    OrderBook book;
    book.add(1, Side::Buy, 101, 50);
    book.add(2, Side::Buy, 100, 30);
    EXPECT_EQ(book.execute(1, 50), 0);
    EXPECT_EQ(book.best_bid().price, 100);
}

TEST(Execute, OverfillRejected) {
    OrderBook book;
    book.add(1, Side::Buy, 100, 50);
    EXPECT_EQ(book.execute(1, 51), 1);
    EXPECT_EQ(book.best_bid().quantity, 50);
}
