#pragma once
#include<vector>
#include<cstdint>
#include<list>
#include<unordered_map>
#include "types.hpp"

static constexpr Price BASE = 90;
static constexpr size_t WINDOW_SIZE = 21;

class FlatOrderBook {
    std::vector<Level> bids;
    std::vector<Level> asks;
    std::unordered_map<OrderId, Location> id_index;

    bool in_window(Price price) const {
        return price >= BASE && price < BASE + static_cast<Price>(WINDOW_SIZE);
    }
    
    public:
        FlatOrderBook() {
            bids.resize(WINDOW_SIZE);
            asks.resize(WINDOW_SIZE);
        }
        int add(OrderId order_id, Side side, Price price, Quantity quantity);
        int cancel(OrderId order_id);
        int execute(OrderId order_id, Quantity quantity);
        Quote best_bid();
        Quote best_ask();
};