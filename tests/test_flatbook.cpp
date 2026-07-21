#include "flatbook.hpp"

#include <gtest/gtest.h>

TEST(FlatBook, BestQuotesReadBack) {
    FlatOrderBook book;
    book.add(1, Side::Buy,  100, 50);
    book.add(2, Side::Buy,  101, 30);
    book.add(3, Side::Sell, 105, 40);
    book.add(4, Side::Sell, 104, 20);

    EXPECT_EQ(book.best_bid().price, 101);
    EXPECT_EQ(book.best_bid().quantity, 30);
    EXPECT_EQ(book.best_ask().price, 104);
    EXPECT_EQ(book.best_ask().quantity, 20);
}

TEST(FlatBook, EmptySentinel) {
    FlatOrderBook book;
    EXPECT_EQ(book.best_bid().price, -1);
    EXPECT_EQ(book.best_ask().price, -1);
}

TEST(FlatBook, VolumeSumsAtLevel) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 50);
    book.add(2, Side::Buy, 100, 30);
    EXPECT_EQ(book.best_bid().quantity, 80);
}

TEST(FlatBook, RejectsOutOfWindowPrice) {
    FlatOrderBook book;
    EXPECT_EQ(book.add(1, Side::Buy, 200, 50), 1);
    EXPECT_EQ(book.best_bid().price, -1);
}

TEST(FlatExecute, PartialFillReducesAndKeepsOrder) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 100);
    EXPECT_EQ(book.execute(1, 30), 0);
    EXPECT_EQ(book.best_bid().quantity, 70);
}

TEST(FlatExecute, FullFillRemovesOrder) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 50);
    book.add(2, Side::Buy, 100, 20);
    EXPECT_EQ(book.execute(1, 50), 0);
    EXPECT_EQ(book.best_bid().quantity, 20);
}

TEST(FlatExecute, FullFillEmptyingLevelMovesTouch) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 101, 50);
    book.add(2, Side::Buy, 100, 30);
    EXPECT_EQ(book.execute(1, 50), 0);
    EXPECT_EQ(book.best_bid().price, 100);
}

TEST(FlatExecute, OverfillRejected) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 50);
    EXPECT_EQ(book.execute(1, 51), 1);
    EXPECT_EQ(book.best_bid().quantity, 50);
}

TEST(FlatExecute, UnknownIdReturnsError) {
    FlatOrderBook book;
    EXPECT_EQ(book.execute(999, 10), 1);
}

TEST(FlatCancel, CancelTwiceReturnsError) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 50);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.cancel(1), 1);
}