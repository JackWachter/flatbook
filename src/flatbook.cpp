#include "flatbook.hpp"

int FlatOrderBook::add(OrderId order_id, Side side, Price price, Quantity quantity) {
    if (!initialized) {
        base = price - static_cast<Price>(WINDOW_SIZE / 2);
        initialized = true;
    }
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
        set_occupied(side, index);
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

    if (bid_mask != 0) {
        uint64_t rotated = std::rotr(bid_mask, static_cast<int>(head));
        size_t offset = 63 - static_cast<size_t>(std::countl_zero(rotated));
        size_t slot = (head + offset) % WINDOW_SIZE;
        best = Quote{base + static_cast<Price>(offset), bids[slot].total_volume};
        found = true;
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

    if (ask_mask != 0) {
        uint64_t rotated = std::rotr(ask_mask, static_cast<int>(head));
        size_t offset = static_cast<size_t>(std::countr_zero(rotated));
        size_t slot = (head + offset) % WINDOW_SIZE;
        best = Quote{base + static_cast<Price>(offset), asks[slot].total_volume};
        found = true;
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
    auto it = id_index.find(order_id);
    if(it == id_index.end()) {
        return 1;
    }

    Location location = it->second;
    Level* level_ptr;
    size_t index= 0;
    if (in_window(location.price)) {
        index = (head + static_cast<size_t>(location.price - base)) % WINDOW_SIZE;
        level_ptr = (location.side == Side::Buy) ? &bids[index] : &asks[index];
    } else {
        auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
        level_ptr = &book[location.price];
    }
    Level& level = *level_ptr;  
    Quantity volume = location.node->quantity;
    level.orders.erase(location.node);
    level.total_volume -= volume;
    id_index.erase(it);
    if (level.orders.empty()) {
        if (!in_window(location.price)) {
            auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
            book.erase(location.price);
        } else {
            clear_occupied(location.side, index);
        }
    }
    return 0;
}

int FlatOrderBook::execute(OrderId order_id, Quantity quantity) {
    auto it = id_index.find(order_id);
    if(it == id_index.end()) {
        return 1;
    }

    Location location = it->second;
    Level* level_ptr;
    size_t index= 0;
    if (in_window(location.price)) {
        index = (head + static_cast<size_t>(location.price - base)) % WINDOW_SIZE;
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
        id_index.erase(it);
        level.orders.erase(location.node);
        if (level.orders.empty()) {
            if (!in_window(location.price)) {
                auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
                book.erase(location.price);
            } else {
                clear_occupied(location.side, index);
            }
        }
        return 0;
    } else {
        return 1;
    }
}

void FlatOrderBook::slide_up() {
    for (size_t offset = 0; offset < CHUNK; offset++) {
        size_t index = (head + offset) % WINDOW_SIZE;
        Level& bid_level = bids[index];
        Level& ask_level = asks[index];

        while (!bid_level.orders.empty()) {
            migrate_order(bid_level, bid_level.orders.begin(), spillover_bids[base + static_cast<Price>(offset)]);
        }
        clear_occupied(Side::Buy, index);
        while (!ask_level.orders.empty()) {
            migrate_order(ask_level, ask_level.orders.begin(), spillover_asks[base + static_cast<Price>(offset)]);
        }
        clear_occupied(Side::Sell, index);

        auto bid_it = spillover_bids.find(base + static_cast<Price>(WINDOW_SIZE + offset));
        if (bid_it != spillover_bids.end()) {
            Level& spill_bid = bid_it->second;
            while (!spill_bid.orders.empty()) {
                migrate_order(spill_bid, spill_bid.orders.begin(), bid_level);
            }
            spillover_bids.erase(bid_it);
            set_occupied(Side::Buy, index);
        }

        auto ask_it = spillover_asks.find(base + static_cast<Price>(WINDOW_SIZE + offset));
        if (ask_it != spillover_asks.end()) {
            Level& spill_ask = ask_it->second;
            while (!spill_ask.orders.empty()) {
                migrate_order(spill_ask, spill_ask.orders.begin(), ask_level);
            }
            spillover_asks.erase(ask_it);
            set_occupied(Side::Sell, index);
        }
    }

    base += CHUNK;
    head = (head + CHUNK) % WINDOW_SIZE;
    slides_up++;
}

void FlatOrderBook::slide_down() {
    for (size_t offset = 0; offset < CHUNK; offset++) {
        size_t index = (head + WINDOW_SIZE - 1 - offset) % WINDOW_SIZE;
        Level& bid_level = bids[index];
        Level& ask_level = asks[index];

        while (!bid_level.orders.empty()) {
            migrate_order(bid_level, bid_level.orders.begin(), spillover_bids[base + static_cast<Price>(WINDOW_SIZE - 1 - offset)]);
        }
        clear_occupied(Side::Buy, index);
        while (!ask_level.orders.empty()) {
            migrate_order(ask_level, ask_level.orders.begin(), spillover_asks[base + static_cast<Price>(WINDOW_SIZE - 1 - offset)]);
        }
        clear_occupied(Side::Sell, index);

        auto bid_it = spillover_bids.find(base - static_cast<Price>(1 + offset));
        if (bid_it != spillover_bids.end()) {
            Level& spill_bid = bid_it->second;
            while (!spill_bid.orders.empty()) {
                migrate_order(spill_bid, spill_bid.orders.begin(), bid_level);
            }
            spillover_bids.erase(bid_it);
            set_occupied(Side::Buy, index);
        }

        auto ask_it = spillover_asks.find(base - static_cast<Price>(1 + offset));
        if (ask_it != spillover_asks.end()) {
            Level& spill_ask = ask_it->second;
            while (!spill_ask.orders.empty()) {
                migrate_order(spill_ask, spill_ask.orders.begin(), ask_level);
            }
            spillover_asks.erase(ask_it);
            set_occupied(Side::Sell, index);
        }
    }

    base -= CHUNK;
    head = (head + WINDOW_SIZE - CHUNK) % WINDOW_SIZE;
    slides_down++;
}

void FlatOrderBook::migrate_order(Level& from, std::list<Order>::iterator node, Level& to) {
    Order order = *node;
    to.orders.push_back(order);
    to.total_volume += order.quantity;
    from.total_volume -= order.quantity;
    auto new_node = std::prev(to.orders.end());
    from.orders.erase(node);
    id_index[order.order_id].node = new_node;
    orders_migrated++;
}
