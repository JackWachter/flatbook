#include "flatbook.hpp"

int FlatOrderBook::add(OrderId order_id, Side side, Price price, Quantity quantity) {
    if (!initialized) {
        base = price - static_cast<Price>(WINDOW_SIZE / 2);
        initialized = true;
    }
    if (!in_window(price)) {
        if (chunk_above(price)) {
            slide_up();
        } else if (chunk_below(price)) {
            slide_down();
        }
    }

    uint32_t idx = alloc_node(order_id, quantity);
    PooledLevel* level_ptr;

    if (in_window(price)) {
        size_t index = (head + static_cast<size_t>(price - base)) % WINDOW_SIZE;
        level_ptr = (side == Side::Buy) ? &bids[index] : &asks[index];
        set_occupied(side, index);
    } else {
        level_ptr = (side == Side::Buy) ? &spillover_bids[price] : &spillover_asks[price];
    }
    PooledLevel& level = *level_ptr;
    link_back(level, idx);
    level.total_volume += quantity;
    id_index[order_id] = PooledLocation{side, price, idx};
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

    PooledLocation location = it->second;
    PooledLevel* level_ptr;
    size_t index= 0;
    if (in_window(location.price)) {
        index = (head + static_cast<size_t>(location.price - base)) % WINDOW_SIZE;
        level_ptr = (location.side == Side::Buy) ? &bids[index] : &asks[index];
    } else {
        auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
        level_ptr = &book[location.price];
    }
    PooledLevel& level = *level_ptr;  
    Quantity volume = pool[location.node_idx].quantity;
    unlink(level, location.node_idx);
    free_node(location.node_idx);
    level.total_volume -= volume;
    id_index.erase(it);
    if (level.head == NIL) {
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

    PooledLocation location = it->second;
    PooledLevel* level_ptr;
    size_t index= 0;
    if (in_window(location.price)) {
        index = (head + static_cast<size_t>(location.price - base)) % WINDOW_SIZE;
        level_ptr = (location.side == Side::Buy) ? &bids[index] : &asks[index];
    } else {
        auto& book = (location.side == Side::Buy) ? spillover_bids : spillover_asks;
        level_ptr = &book[location.price];
    }
    PooledLevel& level = *level_ptr; 
    Quantity volume = pool[location.node_idx].quantity;

    if (volume > quantity) {
        level.total_volume -= quantity;
        pool[location.node_idx].quantity -= quantity;
        return 0;
    } else if (volume == quantity) {
        level.total_volume -= quantity;
        id_index.erase(it);
        unlink(level, location.node_idx);
        free_node(location.node_idx);
        if (level.head == NIL) {
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
        PooledLevel& bid_level = bids[index];
        PooledLevel& ask_level = asks[index];

        Price out_price = base + static_cast<Price>(offset);
        Price in_price  = base + static_cast<Price>(WINDOW_SIZE + offset);
        uint32_t idx = bid_level.head;

        while (idx != NIL) {
            uint32_t next = pool[idx].next;
            migrate_order(bid_level, idx, spillover_bids[out_price]);
            idx = next;
        }
        clear_occupied(Side::Buy, index);

        idx = ask_level.head;
        while (idx != NIL) {
            uint32_t next = pool[idx].next;
            migrate_order(ask_level, idx, spillover_asks[out_price]);
            idx = next;
        }
        clear_occupied(Side::Sell, index);

        auto bid_it = spillover_bids.find(in_price);
        if (bid_it != spillover_bids.end()) {
            idx = bid_it->second.head;
            while (idx != NIL) {
                uint32_t next = pool[idx].next;
                migrate_order(bid_it->second, idx, bid_level);
                idx = next;
            }
            spillover_bids.erase(bid_it);
            set_occupied(Side::Buy, index);
        }

        auto ask_it = spillover_asks.find(base + static_cast<Price>(WINDOW_SIZE + offset));
        if (ask_it != spillover_asks.end()) {
            idx = ask_it->second.head;
            while (idx != NIL) {
                uint32_t next = pool[idx].next;
                migrate_order(ask_it->second, idx, ask_level);
                idx = next;
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
        PooledLevel& bid_level = bids[index];
        PooledLevel& ask_level = asks[index];

        Price out_price = base + static_cast<Price>(WINDOW_SIZE - 1 - offset);
        Price in_price  = base - static_cast<Price>(1 + offset);
        uint32_t idx = bid_level.head;

        while (idx != NIL) {
            uint32_t next = pool[idx].next;
            migrate_order(bid_level, idx, spillover_bids[out_price]);
            idx = next;
        }
        clear_occupied(Side::Buy, index);

        idx = ask_level.head;
        while (idx != NIL) {
            uint32_t next = pool[idx].next;
            migrate_order(ask_level, idx, spillover_asks[out_price]);
            idx = next;
        }
        clear_occupied(Side::Sell, index);

        auto bid_it = spillover_bids.find(in_price);
        if (bid_it != spillover_bids.end()) {
            idx = bid_it->second.head;
            while (idx != NIL) {
                uint32_t next = pool[idx].next;
                migrate_order(bid_it->second, idx, bid_level);
                idx = next;
            }
            spillover_bids.erase(bid_it);
            set_occupied(Side::Buy, index);
        }

        auto ask_it = spillover_asks.find(in_price);
        if (ask_it != spillover_asks.end()) {
            idx = ask_it->second.head;
            while (idx != NIL) {
                uint32_t next = pool[idx].next;
                migrate_order(ask_it->second, idx, ask_level);
                idx = next;
            }
            spillover_asks.erase(ask_it);
            set_occupied(Side::Sell, index);
        }
    }

    base -= CHUNK;
    head = (head + WINDOW_SIZE - CHUNK) % WINDOW_SIZE;
    slides_down++;
}

void FlatOrderBook::migrate_order(PooledLevel& from, uint32_t idx, PooledLevel& to) {
    Quantity qty = pool[idx].quantity;
    unlink(from, idx);
    link_back(to, idx);
    from.total_volume -= qty;
    to.total_volume += qty;
    orders_migrated++;
}

uint32_t FlatOrderBook::alloc_node(OrderId id, Quantity qty) {
    uint32_t idx;
    if (free_head != NIL) {
        idx = free_head;
        free_head = pool[idx].next;
    } else {
        idx = static_cast<uint32_t>(pool.size());
        pool.push_back(OrderNode{});
    }
    pool[idx].id = id;
    pool[idx].quantity = qty;
    pool[idx].next = NIL;
    pool[idx].prev = NIL;
    return idx;
}

void FlatOrderBook::free_node(uint32_t idx) {
    pool[idx].next = free_head;
    free_head = idx;
}

void FlatOrderBook::link_back(PooledLevel& level, uint32_t idx) {
    pool[idx].next = NIL;
    pool[idx].prev = level.tail;
    if (level.tail != NIL) {
        pool[level.tail].next = idx;
    } else {
        level.head = idx;
    }
    level.tail = idx;
}

void FlatOrderBook::unlink(PooledLevel& level, uint32_t idx) {
    uint32_t p = pool[idx].prev;
    uint32_t n = pool[idx].next;

    if (p != NIL) {
        pool[p].next = n;
    } else {
        level.head = n;
    }
    if (n != NIL) {
        pool[n].prev = p;
    } else {
        level.tail = p;
    }
}