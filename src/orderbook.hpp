#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>
#include "types.hpp"

class OrderBook {
    private:
        std::map<Price, Level> bids;
        std::map<Price, Level> asks;
        std::unordered_map<OrderId, Location> id_index; // This maps each order_id to its price and iterator node
    
    public:
        int add(OrderId order_id, Side side, Price price, Quantity quantity);
        int cancel(OrderId order_id);
        int execute(OrderId order_id, Quantity quantity);
        Quote best_bid();
        Quote best_ask();
};
