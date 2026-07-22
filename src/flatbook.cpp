#include "flatbook.hpp"

int FlatOrderBook::add(OrderId order_id, Side side, Price price, Quantity quantity) {
    Order order {order_id, quantity};
    Level* level_ptr;
    if (!in_window(price)) {
        if (chunk_above(price)) {
            slide_up();
        } else if (chunk_below(price)) {
            slide_down();
        }
    }

    if (in_window(price)) {
        size_t index = (head + static_cast<size_t>(price - base)) % WINDOW_SIZE;
        level_ptr = (side == Side::Buy) ? &bids[index] : &asks[index];
    } else {
        level_ptr = (side == Side::Buy) ? &spillover_bids[price] : &spillover_asks[price];
    }
    Level& level = *level_ptr;
    level.orders.push_back(order);
    auto node = std::prev(level.orders.end());
    level.total_volume += quantity;
    Location location {price, node, side};
    id_index[order_id] = location;
    return 0;
}

Quote FlatOrderBook::best_bid() {
    bool found = false;
    Quote best{-1, 0};

    for (size_t offset = WINDOW_SIZE - 1; offset < WINDOW_SIZE; --offset) {
        size_t slot = (head + offset) % WINDOW_SIZE;
        if (bids[slot].orders.empty()) {
            continue;
        }
        Price price = base + static_cast<Price>(offset);
        best = Quote{price, bids[slot].total_volume};
        found = true;
        break;
    }

    if (!spillover_bids.empty()) {
        auto it = spillover_bids.rbegin();
        if (!found || it->first > best.price) {
            best = Quote{it->first, it->second.total_volume};
        }
    }
    return best;
}

Quote FlatOrderBook::best_ask() {
    bool found = false;
    Quote best{-1, 0};

    for (size_t offset = 0; offset < WINDOW_SIZE; offset++) {
        size_t slot = (head + offset) % WINDOW_SIZE;
        if (asks[slot].orders.empty()) {
            continue;
        }
        Price price = base + static_cast<Price>(offset);
        best = Quote{price, asks[slot].total_volume};
        found = true;
        break;
    }

    if (!spillover_asks.empty()) {
        auto it = spillover_asks.begin();
        if (!found || it->first < best.price) {
            best = Quote{it->first, it->second.total_volume};
        }
    }

    return best;
}

int FlatOrderBook::cancel(OrderId order_id) {
    if(!id_index.count(order_id)) {
        return 1;
    }

    Location location = id_index[order_id];
    Level* level_ptr;
    if (in_window(location.price)) {
        size_t index = (head + static_cast<size_t>(location.price - base)) % WINDOW_SIZE;
        level_ptr = (location.side == Side::Buy) ? &bids[index] : &asks[index];
    } else {
        auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
        level_ptr = &book[location.price];
    }
    Level& level = *level_ptr;  
    Quantity volume = location.node->quantity;
    level.orders.erase(location.node);
    level.total_volume -= volume;
    id_index.erase(order_id);
    if (!in_window(location.price) and level.orders.empty()) {
        auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
        book.erase(location.price);
    }
    return 0;
}

int FlatOrderBook::execute(OrderId order_id, Quantity quantity) {
    if(!id_index.count(order_id)) {
        return 1;
    }

    Location location = id_index[order_id];
    Level* level_ptr;
    if (in_window(location.price)) {
        size_t index = (head + static_cast<size_t>(location.price - base)) % WINDOW_SIZE;
        level_ptr = (location.side == Side::Buy) ? &bids[index] : &asks[index];
    } else {
        auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
        level_ptr = &book[location.price];
    }
    Level& level = *level_ptr; 
    Quantity volume = location.node->quantity;

    if (volume > quantity) {
        level.total_volume -= quantity;
        location.node->quantity -= quantity;
        return 0;
    } else if (volume == quantity) {
        level.total_volume -= volume;
        id_index.erase(order_id);
        level.orders.erase(location.node);
        if (!in_window(location.price) and level.orders.empty()) {
            auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
            book.erase(location.price);
        }
        return 0;
    } else {
        return 1;
    }
}

void FlatOrderBook::slide_up() {
    for (size_t offset = 0; offset < CHUNK; offset++) {
        Level& bid_level = bids[(head + offset) % WINDOW_SIZE];
        Level& ask_level = asks[(head + offset) % WINDOW_SIZE];

        while (!bid_level.orders.empty()) {
            migrate_order(bid_level, bid_level.orders.begin(), spillover_bids[base + static_cast<Price>(offset)]);
        }
        while (!ask_level.orders.empty()) {
            migrate_order(ask_level, ask_level.orders.begin(), spillover_asks[base + static_cast<Price>(offset)]);
        }

        auto bid_it = spillover_bids.find(base + static_cast<Price>(WINDOW_SIZE + offset));
        if (bid_it != spillover_bids.end()) {
            Level& spill_bid = bid_it->second;
            while (!spill_bid.orders.empty()) {
                migrate_order(spill_bid, spill_bid.orders.begin(), bid_level);
            }
            spillover_bids.erase(bid_it);
        }

        auto ask_it = spillover_asks.find(base + static_cast<Price>(WINDOW_SIZE + offset));
        if (ask_it != spillover_asks.end()) {
            Level& spill_ask = ask_it->second;
            while (!spill_ask.orders.empty()) {
                migrate_order(spill_ask, spill_ask.orders.begin(), ask_level);
            }
            spillover_asks.erase(ask_it);
        }
    }

    base += CHUNK;
    head = (head + CHUNK) % WINDOW_SIZE;
}

void FlatOrderBook::slide_down() {
    for (size_t offset = 0; offset < CHUNK; offset++) {
        Level& bid_level = bids[(head + WINDOW_SIZE - 1 - offset) % WINDOW_SIZE];
        Level& ask_level = asks[(head + WINDOW_SIZE - 1 - offset) % WINDOW_SIZE];

        while (!bid_level.orders.empty()) {
            migrate_order(bid_level, bid_level.orders.begin(), spillover_bids[base + static_cast<Price>(WINDOW_SIZE - 1 - offset)]);
        }
        while (!ask_level.orders.empty()) {
            migrate_order(ask_level, ask_level.orders.begin(), spillover_asks[base + static_cast<Price>(WINDOW_SIZE - 1 - offset)]);
        }

        auto bid_it = spillover_bids.find(base - static_cast<Price>(1 + offset));
        if (bid_it != spillover_bids.end()) {
            Level& spill_bid = bid_it->second;
            while (!spill_bid.orders.empty()) {
                migrate_order(spill_bid, spill_bid.orders.begin(), bid_level);
            }
            spillover_bids.erase(bid_it);
        }

        auto ask_it = spillover_asks.find(base - static_cast<Price>(1 + offset));
        if (ask_it != spillover_asks.end()) {
            Level& spill_ask = ask_it->second;
            while (!spill_ask.orders.empty()) {
                migrate_order(spill_ask, spill_ask.orders.begin(), ask_level);
            }
            spillover_asks.erase(ask_it);
        }
    }

    base -= CHUNK;
    head = (head + WINDOW_SIZE - CHUNK) % WINDOW_SIZE;
}

void FlatOrderBook::migrate_order(Level& from, std::list<Order>::iterator node, Level& to) {
    Order order = *node;
    to.orders.push_back(order);
    to.total_volume += order.quantity;
    from.total_volume -= order.quantity;
    auto new_node = std::prev(to.orders.end());
    from.orders.erase(node);
    id_index[order.order_id].node = new_node;
}
