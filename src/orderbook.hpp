#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>

using OrderId = uint64_t;
using Price = int32_t;
using Quantity = uint32_t;

enum class Side {
    Buy,
    Sell
};

struct Quote {
    Price price;
    Quantity quantity;
};

struct Order {
    OrderId order_id;
    Quantity quantity;
};

struct Level {
    std::list<Order> orders;
    Quantity total_volume = 0;
};

struct Location {
    Price price;
    std::list<Order>::iterator node;
    Side side;
};

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
