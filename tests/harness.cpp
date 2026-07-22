#include <fstream>
#include <sstream>
#include <cstdint>
#include "orderbook.hpp"
#include "flatbook.hpp"
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

int replay(FlatOrderBook& book, const std::string& filename) {
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

TEST(Generator, ProducesValidStream) {
    OrderBook book;
    int failures = replay(book, "events.txt");
    EXPECT_EQ(failures, 0);
}

TEST(Differential, BooksAgreeOnEveryEvent) {
    OrderBook     map_book;
    FlatOrderBook flat_book;

    std::ifstream in("events.txt");
    ASSERT_TRUE(in) << "could not open events.txt";

    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        std::istringstream iss(line);
        char action;
        iss >> action;

        if (action == 'A') {
            OrderId id; char side_char; Price price; Quantity qty;
            iss >> id >> side_char >> price >> qty;
            Side side = (side_char == 'B') ? Side::Buy : Side::Sell;
            int a = map_book.add(id, side, price, qty);
            int b = flat_book.add(id, side, price, qty);
            ASSERT_EQ(a, b) << "add return codes differ at line " << line_no << ": " << line;
        } else if (action == 'C') {
            OrderId id;
            iss >> id;
            int a = map_book.cancel(id);
            int b = flat_book.cancel(id);
            ASSERT_EQ(a, b) << "cancel return codes differ at line " << line_no << ": " << line;
        } else if (action == 'E') {
            OrderId id; Quantity qty;
            iss >> id >> qty;
            int a = map_book.execute(id, qty);
            int b = flat_book.execute(id, qty);
            ASSERT_EQ(a, b) << "execute return codes differ at line " << line_no << ": " << line;
        } else {
            FAIL() << "unknown action '" << action << "' at line " << line_no;
        }

        Quote mb = map_book.best_bid(),  fb = flat_book.best_bid();
        Quote ma = map_book.best_ask(),  fa = flat_book.best_ask();

        ASSERT_EQ(mb.price, fb.price) << "best_bid price diverged at line " << line_no << ": " << line;
        ASSERT_EQ(mb.quantity, fb.quantity) << "best_bid volume diverged at line " << line_no << ": " << line;
        ASSERT_EQ(ma.price, fa.price) << "best_ask price diverged at line " << line_no << ": " << line;
        ASSERT_EQ(ma.quantity, fa.quantity) << "best_ask volume diverged at line " << line_no << ": " << line;
    }

    EXPECT_GT(line_no, 0) << "events.txt was empty";
}
