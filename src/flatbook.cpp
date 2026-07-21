#include "flatbook.hpp"

int FlatOrderBook::add(OrderId order_id, Side side, Price price, Quantity quantity) {
    if (!in_window(price)) {
        return 1;
    }
    Order order {order_id, quantity};
    size_t index = static_cast<size_t>(price - BASE);
    Level& level = (side == Side::Buy) ? bids[index] : asks[index];
    level.orders.push_back(order);
    auto node = std::prev(level.orders.end());
    level.total_volume += quantity;
    Location location {price, node, side};
    id_index[order_id] = location;
    return 0;
}

Quote FlatOrderBook::best_bid() {
    for (size_t i = bids.size() - 1; i < bids.size(); --i) {
        if (bids[i].orders.empty()) {
            continue;
        }
        Price price = static_cast<Price>(BASE + i);
        Quantity volume = bids[i].total_volume;
        return Quote{price, volume};
    }
    return Quote{-1, 0};
}

Quote FlatOrderBook::best_ask() {
    for (size_t i = 0; i < asks.size(); i++) {
        if (asks[i].orders.empty()) {
            continue;
        }
        Price price = static_cast<Price>(BASE + i);
        Quantity volume = asks[i].total_volume;
        return Quote{price, volume};
    }
    return Quote{-1, 0};
}

int FlatOrderBook::cancel(OrderId order_id) {
    if(!id_index.count(order_id)) {
        return 1;
    }
    Location location = id_index[order_id];
    auto& book = (location.side == Side::Buy) ? bids : asks;
    Level& level = book[static_cast<size_t>(location.price - BASE)];
    Quantity volume = location.node->quantity;
    level.orders.erase(location.node);
    level.total_volume -= volume;
    id_index.erase(order_id);
    return 0;
}

int FlatOrderBook::execute(OrderId order_id, Quantity quantity) {
    if(!id_index.count(order_id)) {
        return 1;
    }
    Location location = id_index[order_id];
    auto& book = (location.side == Side::Buy) ? bids : asks;
    Level& level = book[static_cast<size_t>(location.price - BASE)];
    Quantity volume = location.node->quantity;
    if (volume > quantity) {
        level.total_volume -= quantity;
        location.node->quantity -= quantity;
        return 0;
    } else if (volume == quantity) {
        level.total_volume -= volume;
        id_index.erase(order_id);
        level.orders.erase(location.node);
        return 0;
    } else {
        return 1;
    }
}