#include "orderbook.hpp"

int OrderBook::add(OrderId order_id, Side side, Price price, Quantity quantity) {
    Order order {order_id, quantity};
    Level& level = (side == Side::Buy) ? bids[price] : asks[price];
    level.orders.push_back(order);
    auto node = std::prev(level.orders.end());
    level.total_volume += quantity;
    Location location {price, node, side};
    id_index[order_id] = location;
    return 0;
}

// Returns a quote closest to the touch
Quote OrderBook::best_bid() {
    if (bids.empty()) {
        return Quote{-1, 0};
    }
    auto best_bid = bids.rbegin();
    Price price = best_bid->first;
    Level& level = best_bid->second;
    Quantity volume = level.total_volume;
    return Quote{price, volume};
}

Quote OrderBook::best_ask() {
    if (asks.empty()) {
        return Quote{-1, 0};
    }
    auto best_ask = asks.begin();
    Price price = best_ask->first;
    Level& level = best_ask->second;
    Quantity volume = level.total_volume;
    return Quote{price, volume};
}

int OrderBook::cancel(OrderId order_id) {
    if (!id_index.count(order_id)) {
        return 1;
    }
    Location location = id_index[order_id];
    auto& book = (location.side == Side::Buy) ? bids : asks;
    Level& level = book[location.price];
    Quantity volume = location.node->quantity;
    level.orders.erase(location.node);
    level.total_volume -= volume;
    id_index.erase(order_id);
    if (level.orders.empty()) {
        book.erase(location.price);
    }
    return 0;
}

int OrderBook::execute(OrderId order_id, Quantity quantity) {
    if (!id_index.count(order_id)) {
        return 1;
    }
    Location location = id_index[order_id];
    auto& book = (location.side == Side::Buy) ? bids : asks;
    Level& level = book[location.price];
    Quantity volume = location.node->quantity;
    if (volume < quantity) {
        return 1;
    } else if (volume == quantity) {
        level.total_volume -= quantity;
        id_index.erase(order_id);
        level.orders.erase(location.node);
        if (level.orders.empty()) {
            book.erase(location.price);
        }
        return 0;
    } else {
        level.total_volume -= quantity;
        location.node->quantity -= quantity;
        return 0;
    }
}
