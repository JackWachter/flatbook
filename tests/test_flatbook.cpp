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

TEST(FlatBook, AcceptsOutOfWindowPriceIntoSpillover) {
    FlatOrderBook book;
    EXPECT_EQ(book.add(1, Side::Buy, 200, 50), 0);
    EXPECT_EQ(book.cancel(1), 0);
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

TEST(FlatSlide, SpilloverOrdersPulledBackIn) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 200, 40);

    book.slide_up();
    book.slide_up();
    book.slide_up();

    EXPECT_EQ(book.best_bid().price, 200);
    EXPECT_EQ(book.best_bid().quantity, 40);
}

TEST(FlatSlide, AddAboveWindowTriggersSlideUp) {
    FlatOrderBook book;
    EXPECT_EQ(book.add(1, Side::Buy, 160, 50), 0);
    EXPECT_EQ(book.best_bid().price, 160);
    EXPECT_EQ(book.best_bid().quantity, 50);
}

TEST(FlatSlide, AddBelowWindowTriggersSlideDown) {
    FlatOrderBook book;
    EXPECT_EQ(book.add(1, Side::Buy, 80, 50), 0);
    EXPECT_EQ(book.best_bid().price, 80);
    EXPECT_EQ(book.best_bid().quantity, 50);
}

TEST(FlatSlide, BoundaryPriceLandsInArrayNotSpillover) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 169, 50);
    EXPECT_EQ(book.best_bid().price, 169);
}

TEST(FlatSlide, JustBeyondBoundaryGoesToSpillover) {
    FlatOrderBook book;
    Price before = book.current_base();
    book.add(1, Side::Buy, 170, 50);
    EXPECT_EQ(book.current_base(), before);
    EXPECT_EQ(book.spillover_size(), 1);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.spillover_size(), 0);
}

TEST(FlatSlide, EvictedOrderStillFindableAfterTriggeredSlide) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 30);
    book.add(2, Side::Buy, 160, 50);
    EXPECT_EQ(book.best_bid().price, 160);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.cancel(2), 0);
}

TEST(FlatSlide, SlideDownEvictsTopOfWindow) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 150, 40);
    EXPECT_EQ(book.spillover_size(), 0);
    book.slide_down();
    EXPECT_EQ(book.spillover_size(), 1);
    EXPECT_EQ(book.cancel(1), 0);
}

TEST(FlatSlide, RoundTripRestoresOrder) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 25);
    book.slide_up();
    book.slide_down();
    EXPECT_EQ(book.best_bid().price, 100);
    EXPECT_EQ(book.best_bid().quantity, 25);
}

TEST(FlatSlide, HeadWrapsCorrectly) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 120, 15);
    for (int i = 0; i < 4; i++) book.slide_up();
    for (int i = 0; i < 4; i++) book.slide_down();
    EXPECT_EQ(book.best_bid().price, 120);
    EXPECT_EQ(book.best_bid().quantity, 15);
}

TEST(FlatSlide, BothSidesMigrateTogether) {
    FlatOrderBook book;
    book.add(1, Side::Buy,  95, 10);
    book.add(2, Side::Sell, 95, 20);
    book.slide_up();
    EXPECT_EQ(book.spillover_size(), 2);
    EXPECT_EQ(book.cancel(1), 0);
    EXPECT_EQ(book.cancel(2), 0);
    EXPECT_EQ(book.spillover_size(), 0);
}

TEST(FlatSlide, VolumeCorrectAfterMigration) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 100, 30);
    book.add(2, Side::Buy, 100, 45);
    book.slide_up();
    book.slide_down();
    EXPECT_EQ(book.best_bid().price, 100);
    EXPECT_EQ(book.best_bid().quantity, 75);
}

TEST(FlatSlide, BaseMovesByChunk) {
    FlatOrderBook book;
    Price start = book.current_base();
    book.slide_up();
    EXPECT_EQ(book.current_base(), start + static_cast<Price>(CHUNK));
    book.slide_down();
    EXPECT_EQ(book.current_base(), start);
}

TEST(FlatSlide, TouchCorrectWithNonZeroHead) {
    FlatOrderBook book;
    Price start = book.current_base();

    book.add(1, Side::Buy, 160, 50);
    EXPECT_EQ(book.current_base(), start + static_cast<Price>(CHUNK));

    book.add(2, Side::Buy, 110, 10);
    book.add(3, Side::Buy, 165, 20);
    book.add(4, Side::Sell, 108, 30);
    book.add(5, Side::Sell, 168, 40);

    EXPECT_EQ(book.best_bid().price, 165);
    EXPECT_EQ(book.best_bid().quantity, 20);
    EXPECT_EQ(book.best_ask().price, 108);
    EXPECT_EQ(book.best_ask().quantity, 30);
}

TEST(FlatSlide, TouchCorrectAfterSlideDown) {
    FlatOrderBook book;
    book.add(1, Side::Buy, 80, 50);
    book.add(2, Side::Buy, 100, 10);
    book.add(3, Side::Buy, 76,  20);

    EXPECT_EQ(book.best_bid().price, 100);
    EXPECT_EQ(book.best_bid().quantity, 10);
}